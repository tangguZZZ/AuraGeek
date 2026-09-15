#pragma once
#include <ArduinoJson.h>
#include <Preferences.h>
#include "services/PortalTextRules.h"
#include "services/PortalNetworkConfig.h"

namespace aurageek::services {
struct NetworkCredentials {
  String ssid,password;
  bool valid()const{
    return PortalTextRules::printable(std::string(ssid.c_str(),ssid.length()),32)&&PortalTextRules::wifiPassword(std::string(password.c_str(),password.length()));
  }
  bool load(){Preferences p;if(!p.begin("aura-net",true))return false;String raw=p.isKey("network")?p.getString("network",""):String();p.end();JsonDocument d;PortalNetworkConfig decoded;if(raw.length()>512||deserializeJson(d,raw,DeserializationOption::NestingLimit(2))||!decoded.decode(d.as<JsonVariantConst>()))return false;ssid=decoded.ssid.c_str();password=decoded.password.c_str();return true;}
  bool save()const{if(!valid())return false;JsonDocument d;d["ssid"]=ssid;d["password"]=password;String raw;serializeJson(d,raw);Preferences p;if(!p.begin("aura-net",false))return false;bool ok=p.putString("network",raw)==raw.length()&&p.getString("network","")==raw;p.end();return ok;}
};
}
