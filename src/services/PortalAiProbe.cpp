#include "services/PortalAiProbe.h"
#include "services/VoiceSocket.h"
#include "services/VoiceIdentity.h"
#include "services/VoiceHello.h"
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <ctime>
#include <memory>

namespace aurageek::services {
bool PortalAiProbe::start(const PortalConfig& config,String& error){
  if(active()){error="已有 AI 测试正在进行";return false;}
  if(WiFi.status()!=WL_CONNECTED){error="请先在网络接入中测试并连接家庭 Wi-Fi，再测试 AI";return false;}
  if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<48000||heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<16384){error="内部内存不足，未启动测试；请退出后重新进入配置";return false;}
  Preferences prefs;
  if(config.customService){
    if(!prefs.begin("aura-web",false)){error="无法读取设备身份";return false;}
    auto id=VoiceIdentity::loadOrCreate(
      [&](){return prefs.isKey("custom-id")?std::string(prefs.getString("custom-id","").c_str()):std::string();},
      [&](const std::string& value){return prefs.putString("custom-id",value.c_str())==value.size();},
      [](auto& bytes){esp_fill_random(bytes.data(),bytes.size());});
    prefs.end();clientId_=id.c_str();url_=config.websocketUrl.c_str();token_=config.websocketToken.c_str();version_=config.protocolVersion;
  }else{
    if(!prefs.begin("aura-ai",true)){error="尚未保存官方绑定，请先完成设备绑定";return false;}
    String stored=prefs.isKey("ws-config")?prefs.getString("ws-config",""):String();
    clientId_=prefs.getString("client-id","");prefs.end();
    JsonDocument d;
    if(deserializeJson(d,stored,DeserializationOption::NestingLimit(5))){error="官方连接配置不可用，请先完成绑定";return false;}
    url_=d["url"]|"";token_=d["token"]|"";version_=d["version"]|1U;
  }
  if(!VoiceIdentity::valid(clientId_.c_str())||version_<1||version_>3||!url_.startsWith("wss://")){
    token_="";error="服务连接或设备身份无效，未发起连接";return false;
  }
  httpStatus_=0;state_=State::Clock;++job_;
  if(xTaskCreate(task,"portal-ai",8192,this,1,nullptr)!=pdPASS){
    token_="";state_=State::MemoryFailed;error="测试任务内存不足，未发起连接";return false;
  }
  return true;
}
void PortalAiProbe::task(void* self){
  auto* probe=static_cast<PortalAiProbe*>(self);
  const State result=probe->run(); // Socket and TLS storage destroyed before publishing completion.
  probe->token_="";probe->url_="";probe->clientId_="";
  probe->state_=result;
  vTaskDelete(nullptr);
}
PortalAiProbe::State PortalAiProbe::run(){
  if(time(nullptr)<1700000000){
    configTime(0,0,"pool.ntp.org","time.cloudflare.com");
    const unsigned start=millis();
    while(time(nullptr)<1700000000&&millis()-start<8000){if(WiFi.status()!=WL_CONNECTED)return State::ConnectFailed;vTaskDelay(pdMS_TO_TICKS(100));}
    if(time(nullptr)<1700000000)return State::ClockFailed;
  }
  state_=State::Connect;
  VoiceSocket socket;
  if(!socket.open(url_,token_,clientId_,version_)){
    int status=socket.upgradeStatus();httpStatus_=status;
    return status==401||status==403?State::Unauthorized:State::ConnectFailed;
  }
  httpStatus_=101;state_=State::Hello;
  JsonDocument hello;VoiceHello::request(hello,version_);
  if(!socket.json(hello))return State::HelloFailed;
  struct Free {void operator()(uint8_t* p)const{heap_caps_free(p);}};
  std::unique_ptr<uint8_t,Free> buffer(static_cast<uint8_t*>(heap_caps_malloc(8192,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)));
  if(!buffer)return State::MemoryFailed;
  bool binary=false;int length=socket.receive(buffer.get(),8192,binary,10000);
  JsonDocument response;
  if(length<=0||binary||deserializeJson(response,buffer.get(),length,DeserializationOption::NestingLimit(5))||!VoiceHello::compatible(response.as<JsonVariantConst>()))return State::HelloFailed;
  return State::Passed;
}
void PortalAiProbe::json(JsonObject out)const{
  const State state=state_.load();const char* code="idle";const char* message="尚未测试 AI 连接";
  switch(state){
    case State::Clock:code="clock";message="正在校准证书校验所需时间…";break;
    case State::Connect:code="connect";message="正在建立证书校验的 WSS 连接…";break;
    case State::Hello:code="hello";message="安全连接已建立，正在验证语音协议…";break;
    case State::Passed:code="passed";message="WSS 与语音协议握手通过；未录音、未发送聊天。完整语音效果仍需退出配置后验证。";break;
    case State::ClockFailed:code="clock_failed";message="网络时间校准失败；请检查家庭网络能否访问互联网，未跳过证书校验。";break;
    case State::ConnectFailed:code="connect_failed";message="WSS 连接失败；请核对地址、网络、受信任证书及服务端状态。";break;
    case State::Unauthorized:code="unauthorized";message="服务端拒绝授权（401/403）；请检查 Token 或官方绑定，未修改原配置。";break;
    case State::HelloFailed:code="hello_failed";message="安全连接已建立，但未收到兼容当前固件的语音 hello；请核对协议与音频参数。";break;
    case State::MemoryFailed:code="memory_failed";message="测试内存不足；请退出后重新进入配置再试。";break;
    default:break;
  }
  out["job"]=job_;out["state"]=code;out["message"]=message;
  out["active"]=state==State::Clock||state==State::Connect||state==State::Hello;
  out["http_status"]=httpStatus_.load();
}
}
