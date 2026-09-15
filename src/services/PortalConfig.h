#pragma once
#include <ArduinoJson.h>
#include "services/PortalTextRules.h"
#include "services/StockPreferences.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace aurageek::services {
// Web-only preferences. Daily DISPLAY/AUDIO/MOTION/SPECTRUM settings deliberately
// remain in UserSettings and cannot be imported through this schema.
struct PortalConfig {
  static constexpr size_t kMaxJsonBytes=4096;
  std::string deviceName="AuraGeek",city="SHENZHEN";
  uint32_t background=0x050505,foreground=0xf2f2f2,accent=0xdfff00;
  double latitude=22.5431,longitude=114.0579;
  int utcOffsetMinutes=480;
  bool wakeEnabled=true,wakeGreeting=true,continuous=true;
  unsigned silenceMs=900,noSpeechMs=8000,maxSpeechSeconds=20;
  // Official binding is retained separately in aura-ai. A custom connection never
  // overwrites it; choosing official again restores that binding.
  bool customService=false;
  std::string websocketUrl,websocketToken;
  unsigned protocolVersion=1;
  StockPreferences stocks;

  static bool plain(const std::string& s,size_t max) {
    return PortalTextRules::printable(s,max);
  }
  static bool color(const char* s,uint32_t& value) {
    if(!s||strlen(s)!=7||*s!='#')return false;
    uint32_t result=0;
    for(int i=1;i<7;++i){char c=s[i];int v=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(v<0)return false;result=(result<<4)|v;}
    value=result;return true;
  }
  static double luminance(uint32_t rgb) {
    auto linear=[](unsigned c){double v=c/255.;return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
    return .2126*linear(rgb>>16)+.7152*linear((rgb>>8)&255)+.0722*linear(rgb&255);
  }
  static double contrast(uint32_t a,uint32_t b){double x=luminance(a),y=luminance(b);return (std::max(x,y)+.05)/(std::min(x,y)+.05);}
  static bool secureUrl(const std::string& url) {
    if(url.compare(0,6,"wss://")||!plain(url,256))return false;
    const auto end=url.find('/',6);const auto authority=url.substr(6,end==std::string::npos?end:end-6);
    if(authority.empty()||url.find_first_of(" @\\\"<>#?{}[]|^`")!=std::string::npos)return false;
    for(size_t i=0;i<url.size();++i){
      unsigned char c=url[i];if(c<33||c>126)return false;
      if(c=='%'){
        auto hex=[](char h){return(h>='0'&&h<='9')||(h>='a'&&h<='f')||(h>='A'&&h<='F');};
        if(i+2>=url.size()||!hex(url[i+1])||!hex(url[i+2]))return false;
        i+=2;
      }
    }
    // The existing transport supports DNS/IPv4 and an optional numeric port, not IPv6 literals.
    auto colon=authority.find(':');auto host=authority.substr(0,colon);
    if(host.empty())return false;
    for(char c:host)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-'))return false;
    size_t labelStart=0;
    for(size_t i=0;i<=host.size();++i)if(i==host.size()||host[i]=='.'){
      if(i==labelStart||i-labelStart>63||host[labelStart]=='-'||host[i-1]=='-')return false;
      labelStart=i+1;
    }
    if(host.find_first_not_of("0123456789.")==std::string::npos){
      unsigned parts=0,value=0,digits=0;
      for(size_t i=0;i<=host.size();++i){
        if(i==host.size()||host[i]=='.'){if(!digits||value>255)return false;++parts;value=digits=0;}
        else{value=value*10+unsigned(host[i]-'0');if(++digits>3)return false;}
      }
      if(parts!=4)return false;
    }
    if(colon!=std::string::npos){auto port=authority.substr(colon+1);if(port.empty()||port.size()>5)return false;unsigned n=0;for(char c:port){if(c<'0'||c>'9')return false;n=n*10+c-'0';}if(n==0||n>65535)return false;}
    return true;
  }
  bool valid(std::string& error,bool storedLegacyText=false)const {
    auto fail=[&](const char* msg){error=msg;return false;};
    if(!(storedLegacyText?plain(deviceName,32):PortalTextRules::label(deviceName,true)))return fail("设备名称最多32字节，支持中英文、数字、空格及 - _ . ( ) · （ ）；不能首尾空格或只填符号");
    // The small on-device city label uses the existing Latin font. The website
    // may have a Chinese device name; weather label is explicitly Latin only.
    if(!(storedLegacyText?plain(city,20):PortalTextRules::label(city,false)))return fail("首页城市标签最多20字节，支持英文/拼音、数字、空格及 - _ . ( )；不能首尾空格或只填符号");
    for(unsigned char c:city)if(c>126)return fail("首页城市标签须为英文/拼音");
    if(contrast(background,foreground)<4.5||contrast(background,accent)<3)return fail("配色对比度不足：文字至少4.5:1，强调色至少3:1");
    if(!std::isfinite(latitude)||!std::isfinite(longitude)||latitude< -90||latitude>90||longitude< -180||longitude>180)return fail("经纬度超出范围");
    if(utcOffsetMinutes< -720||utcOffsetMinutes>840||utcOffsetMinutes%15)return fail("时区须为-720到840分钟，步进15分钟");
    if(silenceMs<600||silenceMs>2400||silenceMs%30||noSpeechMs<5000||noSpeechMs>20000||noSpeechMs%100||maxSpeechSeconds<10||maxSpeechSeconds>30||noSpeechMs>maxSpeechSeconds*1000)return fail("聊天时序超出安全范围或步进不匹配");
    if(protocolVersion<1||protocolVersion>3)return fail("语音协议版本须为1到3");
    if(!websocketUrl.empty()&&!secureUrl(websocketUrl))return fail("服务地址须为有效的wss://地址（验证服务器证书）");
    if(customService&&websocketUrl.empty())return fail("自建服务必须填写WebSocket地址");
    if(websocketToken.size()>512)return fail("服务Token过长");
    for(unsigned char c:websocketToken)if(c<32||c>126)return fail("服务Token含非法字符");
    if(!websocketToken.empty()&&websocketToken.find_first_not_of(' ')==std::string::npos)return fail("服务Token不能只填空格");
    return stocks.valid(error);
  }
  void json(JsonDocument& doc,bool secrets=false)const {
    doc["schema"]=1;doc["identity"]["name"]=deviceName;
    char value[8];auto rgb=[&](const char* key,uint32_t n){snprintf(value,sizeof(value),"#%06lx",(unsigned long)n);doc["appearance"][key]=value;};
    rgb("background",background);rgb("foreground",foreground);rgb("accent",accent);
    doc["location"]["label"]=city;doc["location"]["latitude"]=latitude;doc["location"]["longitude"]=longitude;doc["location"]["utc_offset_minutes"]=utcOffsetMinutes;
    doc["conversation"]["wake_enabled"]=wakeEnabled;doc["conversation"]["wake_greeting"]=wakeGreeting;doc["conversation"]["continuous"]=continuous;
    doc["conversation"]["silence_ms"]=silenceMs;doc["conversation"]["no_speech_ms"]=noSpeechMs;doc["conversation"]["max_speech_seconds"]=maxSpeechSeconds;
    doc["service"]["mode"]=customService?"custom":"official";doc["service"]["url"]=websocketUrl;doc["service"]["version"]=protocolVersion;
    if(secrets)doc["service"]["token"]=websocketToken;
    auto list=doc["stocks"]["symbols"].to<JsonArray>();for(unsigned i=0;i<stocks.count;++i)list.add(stocks.symbols[i]);
    doc["stocks"]["ma_fast"]=stocks.maFast;doc["stocks"]["ma_slow"]=stocks.maSlow;
    doc["stocks"]["show_fast"]=stocks.showFast;doc["stocks"]["show_slow"]=stocks.showSlow;
    // token is omitted from exports and GET, including query-string credentials
    // in a custom URL: private URLs are not allowed to contain query parameters.
  }
  // A read-only import preview; tokens never enter either serialized snapshot.
  void changesFrom(const PortalConfig& previous,JsonArray changes)const {
    JsonDocument before,after;previous.json(before);json(after);
    for(auto group:after.as<JsonObjectConst>()){
      if(!strcmp(group.key().c_str(),"schema"))continue;
      for(auto field:group.value().as<JsonObjectConst>()){
        JsonVariantConst oldValue=before[group.key().c_str()][field.key().c_str()];
        if(oldValue==field.value())continue;
        auto change=changes.add<JsonObject>();
        change["field"]=std::string(group.key().c_str())+"."+field.key().c_str();
        change["before"]=oldValue;change["after"]=field.value();
      }
    }
    if(websocketToken!=previous.websocketToken){
      auto change=changes.add<JsonObject>();change["field"]="service.token";
      change["before"]=previous.websocketToken.empty()?"未设置":"已设置（隐藏）";
      change["after"]=websocketToken.empty()?"清除":"替换为新值（隐藏）";
    }
  }
  // Merge transactionally: never partially change the live config on validation failure.
  bool merge(JsonObjectConst root,std::string& error,bool storedLegacyText=false) {
    PortalConfig next=*this;
    if(root.isNull()){error="配置须为JSON对象";return false;}
    auto keys=[&](JsonObjectConst obj,std::initializer_list<const char*> names){for(auto pair:obj){bool found=false;for(auto name:names)if(pair.key().size()==strlen(name)&&!strcmp(pair.key().c_str(),name))found=true;if(!found){error="存在不支持或格式错误的配置字段";return false;}}return true;};
    if(!keys(root,{"schema","identity","appearance","location","conversation","service","stocks"}))return false;
    if(root["schema"].isNull()||!root["schema"].is<int>()||root["schema"].as<int>()!=1){error="配置schema必须为1";return false;}
    auto str=[&](JsonObjectConst obj,const char* key,std::string& out){if(!obj[key].is<const char*>()){error=std::string("字段须为字符串: ")+key;return false;}const char* s=obj[key].as<const char*>();if(strlen(s)!=obj[key].as<JsonString>().size()){error="字符串不能包含NUL";return false;}out=s;return true;};
    auto num=[&](JsonObjectConst obj,const char* key,auto& out){if(!obj[key].is<decltype(out+0)>()){error=std::string("字段类型错误: ")+key;return false;}out=obj[key].as<decltype(out+0)>();return true;};
    for(auto group:root){const char* name=group.key().c_str();if(!strcmp(name,"schema"))continue;auto obj=group.value().as<JsonObjectConst>();if(obj.isNull()){error="配置分组须为对象";return false;}
      if(!strcmp(name,"identity")){if(!keys(obj,{"name"}))return false;if(obj["name"].isUnbound())continue;if(!str(obj,"name",next.deviceName))return false;}
      if(!strcmp(name,"appearance")){if(!keys(obj,{"background","foreground","accent"}))return false;for(auto p:obj){uint32_t* target=!strcmp(p.key().c_str(),"background")?&next.background:!strcmp(p.key().c_str(),"foreground")?&next.foreground:&next.accent;if(!p.value().is<const char*>()||p.value().as<JsonString>().size()!=7||!color(p.value().as<const char*>(),*target)){error="颜色须为#RRGGBB";return false;}}}
      if(!strcmp(name,"location")){if(!keys(obj,{"label","latitude","longitude","utc_offset_minutes"}))return false;for(auto p:obj){auto k=p.key().c_str();if(!strcmp(k,"label")){if(!str(obj,k,next.city))return false;}else if(!strcmp(k,"latitude")){if(!num(obj,k,next.latitude))return false;}else if(!strcmp(k,"longitude")){if(!num(obj,k,next.longitude))return false;}else if(!num(obj,k,next.utcOffsetMinutes))return false;}}
      if(!strcmp(name,"conversation")){if(!keys(obj,{"wake_enabled","wake_greeting","continuous","silence_ms","no_speech_ms","max_speech_seconds"}))return false;for(auto p:obj){auto k=p.key().c_str();bool* flag=!strcmp(k,"wake_enabled")?&next.wakeEnabled:!strcmp(k,"wake_greeting")?&next.wakeGreeting:!strcmp(k,"continuous")?&next.continuous:nullptr;if(flag){if(!p.value().is<bool>()){error="开关须为布尔值";return false;}*flag=p.value().as<bool>();}else {unsigned* value=!strcmp(k,"silence_ms")?&next.silenceMs:!strcmp(k,"no_speech_ms")?&next.noSpeechMs:&next.maxSpeechSeconds;if(!num(obj,k,*value))return false;}}}
      if(!strcmp(name,"service")){if(!keys(obj,{"mode","url","token","version"}))return false;for(auto p:obj){auto k=p.key().c_str();if(!strcmp(k,"mode")){std::string mode;if(!str(obj,k,mode))return false;if(mode!="official"&&mode!="custom"){error="服务模式须为official或custom";return false;}next.customService=mode=="custom";}else if(!strcmp(k,"url")){if(!str(obj,k,next.websocketUrl))return false;}else if(!strcmp(k,"token")){if(!str(obj,k,next.websocketToken))return false;}else if(!num(obj,k,next.protocolVersion))return false;}}
    }
    if(!root["stocks"].isUnbound()){
      auto obj=root["stocks"].as<JsonObjectConst>();
      if(!keys(obj,{"symbols","ma_fast","ma_slow","show_fast","show_slow"}))return false;
      for(auto p:obj){const char* k=p.key().c_str();
        if(!strcmp(k,"symbols")){
          if(!p.value().is<JsonArrayConst>()||p.value().size()<1||p.value().size()>StockPreferences::capacity){error="自选列表须为1到6个字符串组成的数组";return false;}
          next.stocks.symbols={};next.stocks.count=0;
          for(auto v:p.value().as<JsonArrayConst>()){if(!v.is<const char*>()||strlen(v.as<const char*>())!=v.as<JsonString>().size()){error="标的须为字符串且不能包含NUL";return false;}next.stocks.symbols[next.stocks.count++]=v.as<const char*>();}
        }else if(!strcmp(k,"ma_fast")){if(!num(obj,k,next.stocks.maFast))return false;}
        else if(!strcmp(k,"ma_slow")){if(!num(obj,k,next.stocks.maSlow))return false;}
        else{if(!p.value().is<bool>()){error="均线开关须为布尔值";return false;}(!strcmp(k,"show_fast")?next.stocks.showFast:next.stocks.showSlow)=p.value().as<bool>();}
      }
    }
    if(!next.valid(error,storedLegacyText))return false;
    *this=next;
    return true;
  }
};
}
