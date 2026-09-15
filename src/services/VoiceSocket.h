#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_transport_ssl.h>
#include <esp_transport_ws.h>

namespace aurageek::services {
inline bool safeHeader(const String& value) { return value.indexOf('\r')<0 && value.indexOf('\n')<0; }

class VoiceSocket {
 public:
  esp_transport_handle_t tls=nullptr, ws=nullptr;
  ~VoiceSocket() { if(ws){esp_transport_close(ws);esp_transport_destroy(ws);} if(tls)esp_transport_destroy(tls); }
  bool open(const String& url, const String& token, const String& clientId, int version) {
    if(!url.startsWith("wss://") || !safeHeader(url) || !safeHeader(token) || !safeHeader(clientId))return false;
    int slash=url.indexOf('/',6);
    String authority=slash<0 ? url.substring(6) : url.substring(6,slash);
    String path=slash<0 ? "/" : url.substring(slash);
    int port=443, colon=authority.indexOf(':');
    if(colon>=0){port=authority.substring(colon+1).toInt();authority=authority.substring(0,colon);}
    if(authority.isEmpty() || authority.indexOf('@')>=0 || port<1 || port>65535)return false;
    tls=esp_transport_ssl_init(); if(!tls)return false;
    esp_transport_ssl_crt_bundle_attach(tls,esp_crt_bundle_attach);
    ws=esp_transport_ws_init(tls); if(!ws)return false;
    String mac=WiFi.macAddress(); mac.toLowerCase();
    String headers="Protocol-Version: "+String(version)+"\r\nDevice-Id: "+mac+"\r\nClient-Id: "+clientId+"\r\n";
    if(!token.isEmpty())headers+="Authorization: "+(token.indexOf(' ')<0 ? "Bearer "+token : token)+"\r\n";
    esp_transport_ws_set_path(ws,path.c_str());
    if(esp_transport_ws_set_headers(ws,headers.c_str())!=ESP_OK)return false;
    if(esp_transport_connect(ws,authority.c_str(),port,10000)<0){Serial.printf("[AI] WS connect failed HTTP=%d\n",esp_transport_ws_get_upgrade_request_status(ws));return false;}
    return true;
  }
  int upgradeStatus() const { return ws ? esp_transport_ws_get_upgrade_request_status(ws) : 0; }
  bool send(const void* data,size_t length,bool binary=false) {
    auto opcode=ws_transport_opcodes_t(WS_TRANSPORT_OPCODES_FIN|(binary?WS_TRANSPORT_OPCODES_BINARY:WS_TRANSPORT_OPCODES_TEXT));
    return esp_transport_ws_send_raw(ws,opcode,static_cast<const char*>(data),length,3000)==int(length);
  }
  bool json(JsonDocument& doc) { String text;serializeJson(doc,text);return send(text.c_str(),text.length()); }
  // Reassemble partial TCP reads and fragmented WS messages into a bounded buffer.
  // Timeout/close/malformed/control failure terminates the session, never plays partial Opus.
  int receive(uint8_t* buffer,size_t capacity,bool& binary,unsigned timeout) {
    size_t assembled=0; bool started=false; unsigned began=millis();
    do {
      int n=esp_transport_read(ws,reinterpret_cast<char*>(buffer)+assembled,capacity-assembled,100);
      if(n<0)return -1;
      if(n==0){vTaskDelay(1);continue;}
      const int opcode=int(esp_transport_ws_get_read_opcode(ws))&15;
      const int length=esp_transport_ws_get_read_payload_len(ws);
      if(opcode==WS_TRANSPORT_OPCODES_CLOSE)return -1;
      if(length<n || length<0 || size_t(length)>capacity-assembled)return -1;
      if(!started){if(opcode!=1 && opcode!=2)return -1;binary=opcode==2;started=true;}
      else if(opcode!=0)return -1;
      size_t received=n;
      while(received<size_t(length)) {
        if(millis()-began>timeout)return -1;
        n=esp_transport_read(ws,reinterpret_cast<char*>(buffer)+assembled+received,length-received,100);
        if(n<0)return -1;
        received+=n;
      }
      assembled+=received;
      if(esp_transport_ws_get_fin_flag(ws))return int(assembled);
      if(assembled==capacity)return -1;
    } while(millis()-began<timeout);
    return started ? -1 : 0;
  }
};
}
