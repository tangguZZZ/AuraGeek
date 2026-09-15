#pragma once
#include <cstdint>
#include <cstddef>
namespace aurageek { namespace ui {
struct UserSettings {
 uint8_t version=2,brightness=100,volume=100,transition=0,spectrum=0;
 bool valid() const {return version==2&&brightness>=10&&brightness<=100&&volume<=100&&transition<=1&&spectrum<=1;}
 bool operator==(const UserSettings& b)const{return version==b.version&&brightness==b.brightness&&volume==b.volume&&transition==b.transition&&spectrum==b.spectrum;}
 bool operator!=(const UserSettings& b)const{return !(*this==b);}
 // Version 1's third byte was VISUAL gain, not speaker volume. Never reuse it.
 static bool decode(const uint8_t* bytes,size_t count,UserSettings& out,bool& migrated){
  migrated=false;if(!bytes||count!=5)return false;
  if(bytes[0]!=1&&bytes[0]!=2)return false;
  if(bytes[0]==1&&bytes[2]>200)return false;
  UserSettings candidate;candidate.brightness=bytes[1];candidate.volume=bytes[0]==1?100:bytes[2];
  candidate.transition=bytes[3];candidate.spectrum=bytes[4];if(!candidate.valid())return false;
  out=candidate;migrated=bytes[0]==1;return true;
 }
};
static_assert(sizeof(UserSettings)==5,"NVS settings format must remain explicit");
}}
