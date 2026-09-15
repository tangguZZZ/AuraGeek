#include "drivers/VoiceAudio.h"
#include "drivers/SpeechEndpoint.h"
#include "drivers/VoicePacket.h"
#include "services/VoiceIdentity.h"
#include "services/VoiceSocket.h"
#include <esp_random.h>
#include <esp_vad.h>
#include "board/BoardPins.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_transport_ssl.h>
#include <esp_transport_ws.h>
#include <opus.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <ctime>

// The pinned Opus library uses a single non-threadsafe scratch stack.
// All codec calls are owned exclusively by the voice-audio worker.
extern "C" { extern char* global_stack; extern char* scratch_ptr; }

namespace aurageek::drivers {
namespace {
struct FreeMemory { void operator()(void* p) const { heap_caps_free(p); } };
using Memory=std::unique_ptr<void,FreeMemory>;
Memory allocate(size_t bytes) { return Memory(heap_caps_calloc(1,bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)); }
using Socket=services::VoiceSocket;
}

bool VoiceAudio::chat(bool greetOnWake) {
  greetOnWake=greetOnWake&&config_.wakeGreeting;
  // Reuse one authenticated session for alternating speech turns. USB playback
  // and acoustic wake remain excluded until inactivity/cancel ends the session.
  auto execute=[&]() -> bool {
    if(WiFi.status()!=WL_CONNECTED || time(nullptr)<1700000000){Serial.println("[AI] Wi-Fi/NTP not ready");return false;}
    Preferences prefs;String id;
    JsonDocument config;
    if(config_.customService){
      // Custom servers do not require or modify the official activation namespace.
      if(!prefs.begin("aura-web",false))return false;
      const auto identity=services::VoiceIdentity::loadOrCreate(
        [&](){return prefs.isKey("custom-id")?std::string(prefs.getString("custom-id","").c_str()):std::string();},
        [&](const std::string& value){return prefs.putString("custom-id",value.c_str())==value.size();},
        [](auto& bytes){esp_fill_random(bytes.data(),bytes.size());});
      prefs.end();id=identity.c_str();
      if(id.isEmpty()){Serial.println("[AI] custom client identity unavailable; no connection attempted");return false;}
      config["url"]=config_.websocketUrl;config["token"]=config_.websocketToken;config["version"]=config_.protocolVersion;
    }else{
      if(!prefs.begin("aura-ai",true)||!prefs.isKey("ws-config")){Serial.println("[AI] run ai provision and bind device first");return false;}
      String stored=prefs.getString("ws-config","");id=prefs.getString("client-id","");prefs.end();
      if(deserializeJson(config,stored))return false;
    }
    String url=config["url"]|"",token=config["token"]|"";
    int version=config["version"]|1;
    if(version<1 || version>3 || id.length()!=36)return false;
    Socket socket;
    Serial.println("[AI] opening certificate-verified voice session");
    if(!socket.open(url,token,id,version))return false;
    if(cancelled_)return false;
    auto incoming=allocate(8192), pcmStorage=allocate(5760*sizeof(int16_t)), rawStorage=allocate(960*2*sizeof(int32_t)), packetStorage=allocate(1516);
    auto encoderStorage=allocate(opus_encoder_get_size(1)),decoderStorage=allocate(opus_decoder_get_size(1));
    if(!incoming||!pcmStorage||!rawStorage||!packetStorage||!encoderStorage||!decoderStorage)return false;
    if(!global_stack){scratch_ptr=static_cast<char*>(heap_caps_malloc(60000,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));global_stack=scratch_ptr;}
    if(!global_stack)return false;
    auto* encoder=static_cast<OpusEncoder*>(encoderStorage.get());
    auto* decoder=static_cast<OpusDecoder*>(decoderStorage.get());
    if(opus_encoder_init(encoder,16000,1,OPUS_APPLICATION_VOIP)!=OPUS_OK)return false;
    if(opus_encoder_ctl(encoder,OPUS_SET_COMPLEXITY(0))!=OPUS_OK || opus_encoder_ctl(encoder,OPUS_SET_BITRATE(24000))!=OPUS_OK)return false;
    JsonDocument hello;
    hello["type"]="hello";hello["version"]=version;hello["transport"]="websocket";
    hello["audio_params"]["format"]="opus";hello["audio_params"]["sample_rate"]=16000;
    hello["audio_params"]["channels"]=1;hello["audio_params"]["frame_duration"]=60;
    if(!socket.json(hello))return false;
    bool binary=false;
    int length=socket.receive(static_cast<uint8_t*>(incoming.get()),8192,binary,10000);
    JsonDocument server;
    if(length<=0 || binary || deserializeJson(server,incoming.get(),length))return false;
    String session=server["session_id"]|"";
    unsigned rate=server["audio_params"]["sample_rate"]|24000U;
    if(strcmp(server["type"]|"","hello") || strcmp(server["transport"]|"","websocket") || session.isEmpty() || session.length()>256 || (rate!=16000 && rate!=24000 && rate!=48000))return false;
    if(opus_decoder_init(decoder,rate,1)!=OPUS_OK)return false;
    // Runtime codec smoke test before accessing microphone or uploading PCM.
    auto* probe=static_cast<int16_t*>(pcmStorage.get());
    std::fill(probe,probe+960,0);
    int probeBytes=opus_encode(encoder,probe,960,static_cast<uint8_t*>(packetStorage.get()),1500);
    if(probeBytes<=0 || opus_decode(decoder,static_cast<uint8_t*>(packetStorage.get()),probeBytes,probe,5760,0)!=int(rate*60/1000))return false;
    opus_encoder_ctl(encoder,OPUS_RESET_STATE);opus_decoder_ctl(decoder,OPUS_RESET_STATE);
    Serial.printf("[AI] hello/codec OK: TX=16000 RX=%u protocol=%d\n",rate,version);
    auto control=[&](const char* type,const char* state)->bool {JsonDocument doc;doc["session_id"]=session;doc["type"]=type;if(state)doc["state"]=state;if(state && !strcmp(state,"start") && !strcmp(type,"listen"))doc["mode"]="manual";return socket.json(doc);};
    for(unsigned turn=greetOnWake?0:1;!cancelled_;++turn){
    finishListening_=false;emotion_=0;
    auto* raw=static_cast<int32_t*>(rawStorage.get());auto* pcm=static_cast<int16_t*>(pcmStorage.get());auto* packet=static_cast<uint8_t*>(packetStorage.get());
    if(turn==0){
      // Official listen/detect starts a server greeting in the configured voice.
      // No microphone or user PCM is opened/sent until its playback is drained.
      JsonDocument detected;detected["session_id"]=session;detected["type"]="listen";
      detected["state"]="detect";detected["text"]="你好小陈";
      if(!socket.json(detected))return false;
      chatState_=ChatState::Thinking;
      Serial.println("[AI] WAKE GREETING requested; microphone closed; waiting for server voice");
    }else{
    Serial.printf("[AI] conversation turn=%u; same session\n",turn);
    if(!initializeMic() || i2s_channel_enable(rx_)!=ESP_OK)return false;
    rxEnabled_=true;
    size_t bytes=0;
    // Drop initial microphone transient before declaring listening ready.
    for(unsigned i=0;i<2;++i)if(i2s_channel_read(rx_,raw,7680,&bytes,150)!=ESP_OK)return false;
    if(!control("listen","start"))return false;
    chatState_=ChatState::Listening;
    Serial.printf("[AI] LISTENING: speak now; silence=%ums idle=%ums maximum=%us\n",config_.silenceMs,config_.noSpeechMs,config_.maxSpeechSeconds);
    struct VadGuard {vad_handle_t handle=vad_create(VAD_MODE_2);~VadGuard(){if(handle)vad_destroy(handle);}} vad;
    SpeechEndpoint endpoint(config_.silenceMs,config_.noSpeechMs);
    if(!vad.handle){Serial.println("[AI] VAD allocation failed");return false;}
    unsigned started=millis(),sent=0;float dc=0;
    while(!finishListening_ && !cancelled_ && millis()-started<config_.maxSpeechSeconds*1000) {
      bytes=0;
      if(i2s_channel_read(rx_,raw,7680,&bytes,150)!=ESP_OK || bytes!=7680)return false;
      for(unsigned i=0;i<960;++i){float value=float(raw[i*2]>>16);dc+=0.004f*(value-dc);pcm[i]=int16_t(std::clamp<int32_t>(int32_t(value-dc),-32768,32767));}
      for(unsigned i=0;i<960;i+=480)endpoint.feed(vad_process(vad.handle,pcm+i,16000,30)==VAD_SPEECH);
      unsigned header=VoicePacket::headerSize(version);
      int encoded=opus_encode(encoder,pcm,960,packet+header,1500);
      if(encoded<=0||!VoicePacket::writeHeader(version,packet,1516,unsigned(encoded)))return false;
      if(!socket.send(packet,encoded+header,true))return false;
      ++sent;
      if(endpoint.complete() || endpoint.noSpeechTimeout())break;
    }
    i2s_channel_disable(rx_);rxEnabled_=false;i2s_del_channel(rx_);rx_=nullptr;
    if(cancelled_){control("abort",nullptr);return false;}
    if(!endpoint.heardSpeech() && !finishListening_){control("abort",nullptr);Serial.println("[AI] no sustained speech; returning to local wake");return true;}
    Serial.printf("[AI] endpoint=%s\n",finishListening_.load()?"button":(endpoint.complete()?"silence":"limit"));
    if(!sent || !control("listen","stop"))return false;
    chatState_=ChatState::Thinking;Serial.printf("[AI] THINKING: uploaded %u Opus frames\n",sent);
    }
    if(!setOutputRate(rate) || i2s_channel_enable(tx_)!=ESP_OK)return false;
    txEnabled_=true;
    int16_t zeros[256]{};
    for(unsigned i=0;i<5;++i)if(!output(zeros,256))return false;
    unsigned receivedFrames=0,lastData=millis(),responseStarted=millis();bool speaking=false,turnComplete=false;
    while(!cancelled_ && millis()-lastData<30000 && millis()-responseStarted<120000) {
      length=socket.receive(static_cast<uint8_t*>(incoming.get()),8192,binary,250);
      if(length<0)return false;
      if(!length)continue;
      lastData=millis();
      if(binary) {
        auto* payload=static_cast<uint8_t*>(incoming.get());unsigned header=VoicePacket::headerSize(version);
        if(!VoicePacket::valid(version,payload,unsigned(length)))return false;
        int decoded=opus_decode(decoder,payload+header,length-header,pcm,5760,0);
        if(decoded<=0)return false;
        if(!speaking){digitalWrite(board::kSpeakerEnable,HIGH);gainQ15_=0;speaking=true;chatState_=ChatState::Speaking;Serial.println("[AI] SPEAKING");}
        for(int pos=0;pos<decoded;pos+=256)if(!output(pcm+pos,std::min(256,decoded-pos)))return false;
        ++receivedFrames;
      } else {
        JsonDocument message;
        if(deserializeJson(message,incoming.get(),length))return false;
        const char* type=message["type"]|"";
        if(!strcmp(type,"tts") && !strcmp(message["state"]|"","stop")) {
          for(unsigned i=0;i<5;++i)if(!output(zeros,256))return false;
          Serial.printf(turn==0?"[AI] WAKE GREETING complete: received %u Opus frames\n":"[AI] turn complete: received %u Opus frames\n",receivedFrames);
          turnComplete=receivedFrames>0;
          break;
        }
        if(!strcmp(type,"error") || !strcmp(type,"goodbye")){Serial.println("[AI] server ended session");return false;}
        if(!strcmp(type,"stt"))Serial.println("[AI] speech recognized (transcript not logged)");
        if(!strcmp(type,"llm")){
          const char* value=message["emotion"]|"neutral";
          static const char* names[]={"neutral","happy","laughing","sad","angry","crying","loving","embarrassed","surprised","confused","sleepy","kissy","winking","thinking","cool","delicious","funny","confident","relaxed","shocked","silly"};
          unsigned selected=0;for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);++i)if(!strcmp(value,names[i])){selected=i;break;}
          emotion_=selected;Serial.printf("[AI] emotion=%s\n",names[selected]);
        }
      }
    }
    if(cancelled_){control("abort",nullptr);return false;}
    if(!turnComplete){Serial.println("[AI] response incomplete or timed out");return false;}
    // Mute before reopening the microphone. Short acoustic tail clearance is
    // not AEC and does not enable barge-in while the assistant is speaking.
    silence();
    if(turn>0&&!config_.continuous){Serial.println("[AI] single-turn mode; returning to standby");return true;}
    Serial.println("[AI] reply complete; reopening listening after 350ms acoustic tail");
    for(unsigned wait=0;wait<35 && !cancelled_;++wait)vTaskDelay(pdMS_TO_TICKS(10));
    }
    if(cancelled_)control("abort",nullptr);
    return false;
  };
  const bool ok=execute();
  silence();
  if(rx_){if(rxEnabled_)i2s_channel_disable(rx_);rxEnabled_=false;i2s_del_channel(rx_);rx_=nullptr;}
  if(!ok && !cancelled_)Serial.println("[AI] turn failed; check activation, network and microphone");
  return ok;
}
}
