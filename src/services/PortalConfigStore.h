#pragma once
#include "services/PortalConfig.h"
#include <Preferences.h>

namespace aurageek::services {
class PortalConfigStore {
 public:
  bool load() {
    Preferences prefs;
    if(!prefs.begin("aura-web",true))return true; // first boot has no namespace
    String raw=prefs.isKey("config")?prefs.getString("config",""):String();prefs.end();
    if(raw.isEmpty())return true;
    JsonDocument doc;std::string error;
    // Older firmware allowed more punctuation in names. Preserve that saved
    // text and unrelated colors; every new POST/import still uses strict rules.
    if(raw.length()>PortalConfig::kMaxJsonBytes||deserializeJson(doc,raw,DeserializationOption::NestingLimit(5))){error_="已保存JSON损坏或过长";return false;}
    if(!value_.merge(doc.as<JsonObjectConst>(),error,true)){error_=error.c_str();return false;}
    error_="";
    return true;
  }
  const PortalConfig& value()const{return value_;}
  const String& error()const{return error_;}
  bool save(const PortalConfig& next) {
    std::string error;if(!next.valid(error))return false;
    JsonDocument doc;next.json(doc,true);String raw;serializeJson(doc,raw);
    if(raw.length()>PortalConfig::kMaxJsonBytes)return false;
    Preferences prefs;if(!prefs.begin("aura-web",false))return false;
    bool ok=prefs.putString("config",raw)==raw.length()&&prefs.getString("config","")==raw;
    prefs.end();if(ok)value_=next;return ok;
  }
 private:
  PortalConfig value_;
  String error_;
};
}
