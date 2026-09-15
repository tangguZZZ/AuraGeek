#include "services/PortalService.h"
#include "web/PortalPage.h"
#include "services/PortalSubnets.h"
#include "services/StockRequestPolicy.h"
#include <WiFi.h>
#include <esp_random.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <new>
#include "services/FactoryReset.h"

namespace aurageek::services {
bool PortalService::requestBoot(){Preferences p;if(!p.begin("aura-web",false))return false;bool ok=p.putBool("setup",true)==1;p.end();return ok;}
bool PortalService::consumeBootRequest(){Preferences p;if(!p.begin("aura-web",false))return false;bool requested=p.getBool("setup",false);if(requested&&!p.remove("setup"))requested=false;p.end();return requested;}
bool PortalService::begin(PortalConfigStore& store){
  // Restart/reset must not make an old tab's revision zero valid again.
  // Keep ample headroom for subsequent saves and JavaScript exact integers.
  revision_=1U+(esp_random()&0x3fffffffU);
  store_=&store;lastActivity_=millis();WiFi.persistent(false);WiFi.mode(WIFI_AP_STA);WiFi.setSleep(false);
  hasSavedNetwork_=savedNetwork_.load(); // Do not return the saved password to the browser.
  // A detected subnet collision during a new connection is rejected without saving it.
  apSsid_="AuraGeek-Setup-"+WiFi.macAddress().substring(12);apSsid_.replace(":","");
  // User-requested bench password. Replace with per-device credentials before release.
  apPassword_="88888888";
  char hex[3];for(unsigned i=0;i<24;++i){snprintf(hex,sizeof(hex),"%02x",unsigned(esp_random()&255));token_+=hex;}
  if(!WiFi.softAPConfig(ip_,ip_,IPAddress(255,255,255,0))||!WiFi.softAP(apSsid_.c_str(),apPassword_.c_str(),1,false,2)){
    WiFi.softAPdisconnect(true);Serial.println("[PORTAL] AP start failed");return false;
  }
  address_="http://"+ip_.toString();
  if(!dns_.begin(ip_,53)){dns_.stop();WiFi.softAPdisconnect(true);Serial.println("[PORTAL] DNS start failed");return false;}
  http_.reset(new(std::nothrow) PortalHttp(ip_));
  if(!http_||!http_->begin([this](const PortalHttp::Request& request){route(request);})){
    http_.reset();dns_.stop();WiFi.softAPdisconnect(true);
    Serial.println("[PORTAL] HTTP start failed; configuration service stopped");return false;
  }
  WiFi.onEvent([this](WiFiEvent_t,WiFiEventInfo_t info){disconnectReason_=info.wifi_sta_disconnected.reason;},ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent([this](WiFiEvent_t,WiFiEventInfo_t){gotIp_=true;},ARDUINO_EVENT_WIFI_STA_GOT_IP);
  Serial.printf("[PORTAL] AP-only HTTP/DNS ready at %s; password displayed on LCD only\n",address_.c_str());
  return true;
}
void PortalService::reply(int status,bool ok,const char* message){JsonDocument d;d["ok"]=ok;d[ok?"message":"error"]=message;d["revision"]=revision_;String body;serializeJson(d,body);http_->json(status,body);}
void PortalService::connectNetwork(const NetworkCredentials& network,bool reuseSaved){
  candidate_=network;reuseSavedNetwork_=reuseSaved;
  WiFi.scanDelete();scan_.reset();gotIp_=false;disconnectReason_=0;
  connect_.start(millis());connectionStatus_="connecting";
  WiFi.disconnect(false,false); // process() waits for disconnection before WiFi.begin().
}
void PortalService::route(const PortalHttp::Request& r){
  const bool canonical=r.host==ip_.toString()||r.host==ip_.toString()+":80";
  if(!canonical){if(r.method=="GET"&&!r.path.startsWith("/api/"))http_->respond(302,"text/plain",nullptr,0,false,address_+"/");else reply(403,false,"Host rejected");return;}
  if(r.origin.length()&&r.origin!=address_&&r.origin!=address_+":80"){reply(403,false,"Origin rejected");return;}
  if(r.method=="POST"&&(r.token.length()!=token_.length()||r.token!=token_)){reply(403,false,"会话失效，请重新打开配置页");return;}
  if(r.method=="POST"&&exitAt_){reply(409,false,"配置正在退出，请等待设备重启");return;}
  lastActivity_=millis();
  auto send=[&](JsonDocument& d){String body;serializeJson(d,body);http_->json(200,body);};
  if(r.method=="GET"&&r.path=="/"){http_->respond(200,"text/html; charset=utf-8",web::kPortalPage,web::kPortalPageSize,true);return;}
  if(r.method=="GET"&&r.path=="/api/status"){
    JsonDocument d;d["name"]=store_->value().deviceName;d["firmware"]="AuraGeek portal-1";d["uptime_s"]=millis()/1000;d["internal_free"]=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);d["internal_min"]=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);d["internal_largest"]=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);d["psram_free"]=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);d["connection"]=connectionStatus_;d["reason"]=disconnectReason_.load();d["sta_ip"]=WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():String();d["ap_ip"]=ip_.toString();d["voice_active"]=false;d["token"]=token_;d["revision"]=revision_;
    Preferences prefs;bool configured=false;if(prefs.begin("aura-ai",true)){configured=prefs.isKey("ws-config");prefs.end();}d["official_connection_saved"]=configured;aiProbe_.json(d["ai_test"].to<JsonObject>());
    // Portal mode does not start the stock worker. Read its persisted guard only;
    // inspecting this status must never reserve a request or create NVS keys.
    uint64_t budget=0;uint32_t next=0;bool paused=false;
    Preferences stocks;if(stocks.begin("ag-stocks",true)){
      if(stocks.isKey("globalbudget"))budget=stocks.getULong64("globalbudget",0);
      if(stocks.isKey("globalnext"))next=stocks.getUInt("globalnext",0);
      if(stocks.isKey("providerblocked"))paused=stocks.getBool("providerblocked",false);
      stocks.end();
    }
    auto policy=d["stock_policy"].to<JsonObject>();policy["budget_day"]=uint32_t(budget>>32);policy["budget_used"]=uint32_t(budget);policy["next_allowed_epoch"]=next;policy["paused"]=paused;policy["daily_limit"]=StockRequestPolicy::dailyLimit;policy["min_spacing_seconds"]=StockRequestPolicy::minSpacing;
    d["factory_reset_preserve_binding"]=true;
    d["saved_network"]["available"]=hasSavedNetwork_;d["saved_network"]["ssid"]=hasSavedNetwork_?savedNetwork_.ssid:String();send(d);return;
  }
  if(r.method=="GET"&&r.path=="/api/config"){
    JsonDocument config,d;store_->value().json(config);
    d["revision"]=revision_;d["token"]=token_;d["config"]=config.as<JsonVariantConst>();send(d);return;
  }
  if(r.method=="GET"&&r.path=="/api/wifi/scan"){
    if(connect_.active()||aiProbe_.active()){reply(409,false,"正在测试连接，请稍后扫描");return;}
    JsonDocument d;
    if(scan_.state()==PortalScanState::State::Ready){d["pending"]=false;auto networks=d["networks"].to<JsonArray>();for(int i=0;i<scan_.count()&&i<24;++i){auto n=networks.add<JsonObject>();n["ssid"]=WiFi.SSID(i);n["rssi"]=WiFi.RSSI(i);n["secure"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;}WiFi.scanDelete();scan_.reset();}
    else if(scan_.state()==PortalScanState::State::Failed){scan_.reset();reply(500,false,"扫描失败或超时，请重试");return;}
    else{
      if(!scan_.running()){
        WiFi.scanDelete();scan_.start(millis());
        if(scan_.update(WiFi.scanNetworks(true,true),millis())){
          esp_wifi_scan_stop();WiFi.scanDelete();scan_.reset();reply(500,false,"无法启动扫描，请重试");return;
        }
      }
      d["pending"]=true;
    }
    send(d);return;
  }
  if(r.method=="POST"){
    JsonDocument d;if(deserializeJson(d,r.body,DeserializationOption::NestingLimit(5))||!d.is<JsonObject>()){reply(400,false,"JSON格式无效");return;}
    if(aiProbe_.active()){reply(409,false,"AI 连接测试尚未结束，请等待测试结果再修改或退出");return;}
    if(r.path=="/api/factory-reset"){
      if(connect_.active()||scan_.running()){reply(409,false,"请等待网络操作结束再恢复出厂设置");return;}
      if(r.revision!=String(revision_)){reply(409,false,"配置已变化，请重新读取后确认恢复出厂设置");return;}
      if(d.size()!=2||!d["confirmation"].is<String>()||d["confirmation"].as<String>()!="恢复出厂"||
         !d["preserve_official_binding"].is<bool>()||!d["preserve_official_binding"].as<bool>()){
        reply(400,false,"请输入恢复出厂，并确认保留官方 AI 绑定");return;
      }
      // Stage intent only. Apply at boot before any settings-dependent worker.
      if(!FactoryReset<Preferences>::request()){reply(500,false,"恢复请求写入未确认，请重新连接检查设备状态，不要重复提交");return;}
      exitAt_=millis()+1500;reply(200,true,"恢复请求已确认；设备将重启并进入配网热点，官方 AI 绑定保留");return;
    }
    if(r.path=="/api/ai/test"){
      if(connect_.active()||scan_.running()){reply(409,false,"请等待网络操作完成再测试 AI");return;}
      if(r.revision!=String(revision_)){reply(409,false,"请重新读取配置后再测试 AI");return;}
      if(d.size()!=2||!d["service"].is<JsonObject>()){reply(400,false,"AI 测试只接收 schema 与 service 字段");return;}
      PortalConfig next=store_->value();std::string validation;
      if(!next.merge(d.as<JsonObjectConst>(),validation)){reply(400,false,validation.c_str());return;}
      String error;if(!aiProbe_.start(next,error)){reply(409,false,error.c_str());return;}
      JsonDocument result;result["ok"]=true;result["message"]="测试已开始，不保存服务参数、不录音；请等待结果";aiProbe_.json(result["ai_test"].to<JsonObject>());send(result);return;
    }
    if(r.path=="/api/config"||r.path=="/api/config/validate"){
      if(connect_.active()){reply(409,false,"请等待网络测试完成");return;}
      if(r.revision!=String(revision_)){reply(409,false,"配置已被其他页面修改或尚未读取，请重新读取后再保存；本次修改未写入");return;}
      PortalConfig next=store_->value();std::string error;if(!next.merge(d.as<JsonObjectConst>(),error)){reply(400,false,error.c_str());return;}
      if(r.path=="/api/config/validate"){
        JsonDocument preview;preview["ok"]=true;preview["revision"]=revision_;
        next.changesFrom(store_->value(),preview["changes"].to<JsonArray>());
        send(preview);return; // No NVS write, no live configuration changes.
      }
      if(!store_->save(next)){reply(500,false,"保存失败，当前配置未替换");return;}
      ++revision_;reply(200,true,"已写入并校验；退出配置后生效");return;
    }
    if(r.path=="/api/wifi/connect"){
      if(connect_.active()||scan_.running()){reply(409,false,"已有网络操作正在进行");return;}
      PortalNetworkConfig decoded;if(!decoded.decode(d.as<JsonVariantConst>())){reply(400,false,"网络字段无效：SSID须为1到32字节；密码须为8到63位英文/数字/符号或64位十六进制，开放网络留空");return;}
      NetworkCredentials network;network.ssid=decoded.ssid.c_str();network.password=decoded.password.c_str();
      connectNetwork(network,false);reply(202,true,"正在连接；热点不会因密码错误关闭");return;
    }
    if(r.path=="/api/wifi/reconnect"){
      if(d.size()){reply(400,false,"重连已保存网络不接收SSID或密码");return;}
      if(connect_.active()||scan_.running()){reply(409,false,"已有网络操作正在进行");return;}
      if(!hasSavedNetwork_){reply(409,false,"尚未通过网页保存网络，请先填写一次Wi-Fi名称和密码");return;}
      connectNetwork(savedNetwork_,true);reply(202,true,"正在使用设备已保存的网络；无需重新输入密码，失败也不会删除原凭据");return;
    }
    if(r.path=="/api/exit"){if(connect_.active()){reply(409,false,"请等待网络测试完成");return;}exitAt_=millis()+1500;reply(200,true,"即将退出配置并恢复设备");return;}
  }
  if(r.path.startsWith("/api/")){reply(404,false,"接口不存在");return;}
  if(r.method=="GET"){http_->respond(302,"text/plain",nullptr,0,false,address_+"/");return;}
  reply(404,false,"接口不存在");
}
void PortalService::dns(){
  int size=dns_.parsePacket();if(!size)return;
  uint8_t b[512];if(size<12||size>480){dns_.clear();return;}IPAddress peer=dns_.remoteIP();unsigned port=dns_.remotePort();if(peer[0]!=ip_[0]||peer[1]!=ip_[1]||peer[2]!=ip_[2]){dns_.clear();return;}
  if(dns_.read(b,size)!=size||b[2]&0xf8||b[4]!=0||b[5]!=1)return;
  size_t end=12;while(end<size_t(size)&&b[end]){unsigned n=b[end++];if(n>63||end+n>=size_t(size))return;end+=n;}if(end+5>size_t(size))return;++end;
  bool a=b[end]==0&&b[end+1]==1&&b[end+2]==0&&b[end+3]==1;
  b[2]=0x81;b[3]=0x80;b[6]=0;b[7]=a?1:0;b[8]=b[9]=b[10]=b[11]=0;
  size_t count=end+4;if(a){uint8_t answer[]={0xc0,0x0c,0,1,0,1,0,0,0,0,0,4,ip_[0],ip_[1],ip_[2],ip_[3]};memcpy(b+count,answer,sizeof(answer));count+=sizeof(answer);}
  dns_.beginPacket(peer,port);dns_.write(b,count);dns_.endPacket();
}
void PortalService::process(){
  // Completion must not depend on the phone polling again: closing the page
  // during a scan must not leave connection attempts permanently locked out.
  const bool scanning=scan_.running();
  if(scan_.update(scanning?WiFi.scanComplete():WIFI_SCAN_RUNNING,millis())){
    if(scanning)esp_wifi_scan_stop();
    WiFi.scanDelete();
  }
  dns();http_->process();
  if(connect_.active()){
    const auto action=connect_.step(WiFi.status()==WL_CONNECTED,gotIp_.load(),WiFi.SSID()==candidate_.ssid,millis());
    if(action==PortalConnectState::Action::Begin){
      gotIp_=false;WiFi.begin(candidate_.ssid.c_str(),candidate_.password.c_str());
    }else if(action==PortalConnectState::Action::Passed){
      auto address=[](const IPAddress& ip){return uint32_t(ip[0])<<24|uint32_t(ip[1])<<16|uint32_t(ip[2])<<8|ip[3];};
      const bool overlap=PortalSubnets::overlap(address(WiFi.localIP()),address(WiFi.subnetMask()),address(ip_),0xffffff00U);
      if(overlap){connectionStatus_="subnet_conflict";WiFi.disconnect(false,false);}
      else if(reuseSavedNetwork_)connectionStatus_="connected_existing";
      else if(candidate_.save()){savedNetwork_=candidate_;hasSavedNetwork_=true;connectionStatus_="connected_saved";}
      else connectionStatus_="save_failed";
      candidate_=NetworkCredentials{};
    }else if(action==PortalConnectState::Action::TimedOut){WiFi.disconnect(false,false);connectionStatus_="failed";candidate_=NetworkCredentials{};}
  }else if((connectionStatus_=="connected_saved"||connectionStatus_=="connected_existing"||connectionStatus_=="save_failed")&&WiFi.status()!=WL_CONNECTED){
    connectionStatus_="disconnected"; // Keep the prior credentials; expose loss of connectivity honestly.
  }
  if(millis()-lastHeap_>=10000){lastHeap_=millis();Serial.printf("[PORTAL] internal=%u minimum=%u largest=%u psram=%u connection=%s\n",unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),connectionStatus_.c_str());}
}
}
