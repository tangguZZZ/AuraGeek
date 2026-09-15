#include "services/PortalConfig.h"
#include "services/PortalNetworkConfig.h"
#include "services/PortalConnectState.h"
#include "services/PortalSubnets.h"
#include <cassert>
#include <cstdio>
using aurageek::services::PortalConfig;
int main(){
  using Network=aurageek::services::PortalNetworkConfig;
  Network network{"old-network","old-pass"};
  auto decodeNetwork=[&](const char* raw){JsonDocument d;assert(!deserializeJson(d,raw));return network.decode(d.as<JsonVariantConst>());};
  for(const char* malformed:{R"({"ssid":"x"})",R"({"ssid":"x","password":12345678})",R"({"ssid":"x","password":"12345678","extra":1})",R"({"ssid":"x\u0000y","password":"12345678"})",R"({"ssid":"x","password\u0000suffix":"12345678"})",R"({"ssid":"x","password":"short"})"}){
    assert(!decodeNetwork(malformed));assert(network.ssid=="old-network"&&network.password=="old-pass");
  }
  assert(decodeNetwork(R"({"ssid":"家庭 Wi-Fi","password":"88888888"})"));
  assert(decodeNetwork(R"({"ssid":"Open Wi-Fi","password":""})"));
  assert(network.password.empty());
  using Attempt=aurageek::services::PortalConnectState;using Action=Attempt::Action;
  Attempt attempt;assert(!attempt.active());attempt.start(100);
  assert(attempt.step(true,true,true,101)==Action::Wait); // Old connection cannot approve new credentials.
  assert(attempt.step(false,true,true,102)==Action::Begin);
  assert(attempt.step(true,false,true,103)==Action::Wait); // Must observe a new GOT_IP after begin.
  assert(attempt.step(true,true,false,104)==Action::Wait); // Wrong SSID cannot approve candidate.
  assert(attempt.step(true,true,true,105)==Action::Passed&&!attempt.active());
  assert(attempt.step(true,true,true,106)==Action::Wait); // Success is consumed once.
  attempt.start(200);assert(attempt.step(true,true,true,3200)==Action::TimedOut);
  attempt.start(4000);assert(attempt.step(false,false,false,4001)==Action::Begin);
  assert(attempt.step(false,false,false,24000)==Action::Wait);
  assert(attempt.step(false,false,false,24001)==Action::TimedOut&&!attempt.active());
  attempt.start(0xfffffff0U);assert(attempt.step(false,false,false,0xfffffff1U)==Action::Begin);
  assert(attempt.step(false,false,false,0x4e11U)==Action::TimedOut); // millis wraps safely.
  using aurageek::services::PortalSubnets;
  assert(PortalSubnets::overlap(0xc0a80481,0xffffff80,0xc0a80401,0xffffff00)); // STA /25 overlaps AP /24 even though AP .1 is outside STA range.
  assert(PortalSubnets::overlap(0xc0a80501,0xffff0000,0xc0a80401,0xffffff00));
  assert(!PortalSubnets::overlap(0xc0a80501,0xffffff00,0xc0a80401,0xffffff00));
  assert(!PortalSubnets::overlap(0xc0a80481,0xffffff80,0xc0a80401,0xffffff80));
  PortalConfig config;std::string error;assert(config.valid(error));
  JsonDocument legacy;assert(!deserializeJson(legacy,R"({"schema":1,"identity":{"name":"My <old> device"},"appearance":{"background":"#120c1f","foreground":"#f4edff","accent":"#cfafff"}})"));
  PortalConfig old;assert(old.merge(legacy.as<JsonObjectConst>(),error,true));assert(old.background==0x120c1f);
  assert(!old.valid(error)); // Only loading existing storage permits legacy punctuation.
  PortalConfig imported;assert(!imported.merge(legacy.as<JsonObjectConst>(),error));
  auto merge=[&](const char* json){JsonDocument doc;assert(!deserializeJson(doc,json));return config.merge(doc.as<JsonObjectConst>(),error);};
  assert(merge(R"({"schema":1,"identity":{"name":"小陈 AuraGeek-1"},"appearance":{"background":"#101820","foreground":"#ffffff","accent":"#00ffcc"}})"));
  assert(!merge(R"({"schema":1,"identity":{"name":"Changed"},"appearance":{"background":"#ffffff","foreground":"#ffffff"}})"));
  assert(config.deviceName=="小陈 AuraGeek-1"); // failed transaction did not change name
  for(const char* name:{"   ","---"," AURA","AURA ","<script>","AI\xe2\x80\x8b","AI\xf0\x9f\x98\x80","AI\xc0\xaf","AI\xed\xa0\x80"}){
    PortalConfig invalid=config;invalid.deviceName=name;assert(!invalid.valid(error));
  }
  assert(!merge(R"({"schema":1,"identity":{"name\u0000suffix":"wrong"}})"));
  assert(!merge(R"({"schema":1,"location":{"label":"深圳"}})"));
  assert(!merge(R"({"schema":1,"location":{"label":"!!!"}})"));
  assert(merge(R"({"schema":1,"location":{"label":"Taipei-01"}})"));
  assert(!merge(R"({"schema":1,"conversation":{"no_speech_ms":8001}})"));
  using aurageek::services::PortalTextRules;
  assert(PortalTextRules::printable("家庭 Wi-Fi \xf0\x9f\x98\x80",32));
  assert(!PortalTextRules::printable("Wi-Fi\xe2\x80\xae",32));
  assert(!PortalTextRules::printable(std::string(33,'a'),32));
  assert(PortalTextRules::wifiPassword("88888888"));
  assert(PortalTextRules::wifiPassword("a B!@#$%"));
  assert(!PortalTextRules::wifiPassword("1234567"));
  assert(!PortalTextRules::wifiPassword("中文密码12345678"));
  assert(PortalTextRules::wifiPassword(std::string(64,'a')));
  assert(!PortalTextRules::wifiPassword(std::string(64,'z')));
  assert(!merge(R"({"schema":1,"volume":100})"));
  assert(!merge(R"({"schema":1,"conversation":{"silence_ms":-1}})"));
  assert(!merge(R"({"schema":1,"conversation":{"wake_enabled":1}})"));
  assert(!merge(R"({"schema":1,"conversation":{"silence_ms":900.5}})"));
  assert(!merge(R"({"schema":1,"location":{"latitude":91}})"));
  assert(!merge(R"({"schema":1,"location":{"utc_offset_minutes":481}})"));
  assert(!merge(R"({"schema":1,"identity":{"name":"x\u0000y"}})"));
  assert(!merge(R"({"schema":1,"appearance":{"accent":"#00ffcc\u0000hidden"}})"));
  assert(!merge(R"({"schema":1,"service":{"url":"ws://example.org/chat","mode":"custom"}})"));
  assert(!merge(R"({"schema":1,"service":{"url":"wss://user@example.org/chat"}})"));
  assert(!merge(R"({"schema":1,"service":{"url":"wss://example.org:99999/chat"}})"));
  assert(!PortalConfig::secureUrl("wss://-bad.example/chat"));
  assert(!PortalConfig::secureUrl("wss://bad..example/chat"));
  assert(!PortalConfig::secureUrl("wss://example.org/中文"));
  assert(!PortalConfig::secureUrl("wss://example.org/%xx"));
  assert(!merge(R"({"schema":1,"service":{"token":"hello\r\nInjected"}})"));
  assert(merge(R"({"schema":1,"service":{"mode":"custom","url":"wss://example.org:443/chat","token":"secret","version":3},"conversation":{"continuous":false,"silence_ms":1200}})"));
  assert(!merge(R"({"schema":1,"service":{"url":"wss://example.org/chat?token=secret"}})"));
  JsonDocument publicDoc;config.json(publicDoc);std::string exported;serializeJson(publicDoc,exported);assert(exported.find("secret")==std::string::npos);assert(publicDoc["service"]["token"].isUnbound());
  assert(config.merge(publicDoc.as<JsonObjectConst>(),error));assert(config.websocketToken=="secret");
  JsonDocument privateDoc;config.json(privateDoc,true);PortalConfig restored;assert(restored.merge(privateDoc.as<JsonObjectConst>(),error));assert(restored.websocketToken=="secret"&&restored.protocolVersion==3);
  // NVS is serialized text, not an in-memory JsonDocument. ArduinoJson may
  // parse short decimals through float even when the destination is double.
  std::string persisted;serializeJson(privateDoc,persisted);JsonDocument readback;
  assert(!deserializeJson(readback,persisted));PortalConfig fromFlash;
  assert(fromFlash.merge(readback.as<JsonObjectConst>(),error,true));
  assert(fromFlash.background==config.background&&fromFlash.accent==config.accent);
  assert(std::abs(fromFlash.latitude-config.latitude)<0.00001);
  // Reproduce why a hard four-decimal quantization check rejected real storage.
  assert(std::abs(fromFlash.latitude*10000-std::round(fromFlash.latitude*10000))>0.000001);
  assert(merge(R"({"schema":1,"service":{"mode":"official","token":""}})"));assert(config.websocketToken.empty()&&!config.customService);
  PortalConfig original,changed=original;changed.continuous=false;changed.websocketToken="secret-preview-never-return";
  JsonDocument preview;changed.changesFrom(original,preview.to<JsonArray>());
  assert(preview.size()==2);std::string previewText;serializeJson(preview,previewText);
  assert(previewText.find("conversation.continuous")!=std::string::npos&&previewText.find("service.token")!=std::string::npos);
  assert(previewText.find("secret-preview-never-return")==std::string::npos);
  assert(original.continuous&&original.websocketToken.empty()); // Preview is read-only.
  preview.clear();original.changesFrom(original,preview.to<JsonArray>());assert(preview.size()==0);
  puts("Portal config: validation, transaction, secret redaction and import roundtrip PASS");
}
