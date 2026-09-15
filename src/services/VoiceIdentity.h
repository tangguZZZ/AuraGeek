#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace aurageek::services {
struct VoiceIdentity {
  static bool valid(const std::string& id) {
    if(id.size()!=36)return false;
    for(size_t i=0;i<id.size();++i) {
      if(i==8||i==13||i==18||i==23){if(id[i]!='-')return false;}
      else if(!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f')||(id[i]>='A'&&id[i]<='F')))return false;
    }
    return true;
  }
  template<class Read,class Write,class Random>
  static std::string loadOrCreate(Read read,Write write,Random random) {
    const std::string existing=read();
    // Never silently replace a malformed identity that may already be bound.
    if(!existing.empty())return valid(existing)?existing:std::string();
    std::array<uint8_t,16> bytes{};random(bytes);
    bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;
    static constexpr char hex[]="0123456789abcdef";
    std::string id;id.reserve(36);
    for(size_t i=0;i<bytes.size();++i){
      if(i==4||i==6||i==8||i==10)id+='-';
      id+=hex[bytes[i]>>4];id+=hex[bytes[i]&15];
    }
    return write(id)&&read()==id?id:std::string();
  }
};
}
