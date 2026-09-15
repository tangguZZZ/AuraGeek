#pragma once
#include "services/PortalTextRules.h"
#include <ArduinoJson.h>
#include <cstring>

namespace aurageek::services {
struct PortalNetworkConfig {
  std::string ssid,password;
  bool valid()const{return PortalTextRules::printable(ssid,32)&&PortalTextRules::wifiPassword(password);}
  bool decode(JsonVariantConst value){
    if(!value.is<JsonObjectConst>()||value.size()!=2)return false;
    auto root=value.as<JsonObjectConst>();
    for(auto field:root){auto key=field.key();if(!((key.size()==4&&!strcmp(key.c_str(),"ssid"))||(key.size()==8&&!strcmp(key.c_str(),"password"))))return false;}
    for(const char* key:{"ssid","password"})if(!root[key].is<const char*>()||root[key].as<JsonString>().size()!=strlen(root[key].as<const char*>()))return false;
    PortalNetworkConfig next{root["ssid"].as<const char*>(),root["password"].as<const char*>()};
    if(!next.valid())return false;
    *this=next;return true;
  }
};
}
