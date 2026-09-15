#pragma once
#include <ArduinoJson.h>
#include <cstring>

namespace aurageek::services {
struct VoiceHello {
  static void request(JsonDocument& d, unsigned version) {
    d["type"]="hello";d["version"]=version;d["transport"]="websocket";
    auto audio=d["audio_params"].to<JsonObject>();
    audio["format"]="opus";audio["sample_rate"]=16000;
    audio["channels"]=1;audio["frame_duration"]=60;
  }
  static bool compatible(JsonVariantConst d) {
    if(!d.is<JsonObjectConst>())return false;
    auto exact=[](JsonVariantConst v,const char* expected){
      if(!v.is<const char*>())return false;
      auto s=v.as<JsonString>();return s.size()==strlen(expected)&&!strcmp(s.c_str(),expected);
    };
    if(!exact(d["type"],"hello")||!exact(d["transport"],"websocket"))return false;
    if(!d["session_id"].is<const char*>())return false;
    auto id=d["session_id"].as<JsonString>();
    if(!id.size()||id.size()>256||strlen(id.c_str())!=id.size())return false;
    auto audio=d["audio_params"];
    if(!audio.is<JsonObjectConst>()||!exact(audio["format"],"opus"))return false;
    if(!audio["sample_rate"].is<unsigned>()||!audio["channels"].is<unsigned>()||!audio["frame_duration"].is<unsigned>())return false;
    unsigned rate=audio["sample_rate"].as<unsigned>();
    return (rate==16000||rate==24000||rate==48000)&&audio["channels"].as<unsigned>()==1&&audio["frame_duration"].as<unsigned>()==60;
  }
};
}
