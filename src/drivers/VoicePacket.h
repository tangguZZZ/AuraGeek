#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace aurageek::drivers {
struct VoicePacket {
  static unsigned headerSize(unsigned version){return version==2?16:version==3?4:0;}
  static unsigned read16(const uint8_t* p){return unsigned(p[0])*256+p[1];}
  static uint32_t read32(const uint8_t* p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
  static bool writeHeader(unsigned version,uint8_t* packet,size_t capacity,size_t payloadSize){
    const auto header=headerSize(version);
    if(version<1||version>3||!packet||!payloadSize||payloadSize>1500||capacity<header||payloadSize>capacity-header)return false;
    memset(packet,0,header);
    if(version==2){packet[1]=2;packet[14]=uint8_t(payloadSize>>8);packet[15]=uint8_t(payloadSize);}
    if(version==3){packet[2]=uint8_t(payloadSize>>8);packet[3]=uint8_t(payloadSize);}
    return true;
  }
  static bool valid(unsigned version,const uint8_t* packet,size_t size){
    const auto header=headerSize(version);
    if(version<1||version>3||!packet||size<=header)return false;
    if(version==2)return read16(packet)==2&&read16(packet+2)==0&&read32(packet+12)==size-header;
    if(version==3)return packet[0]==0&&read16(packet+2)==size-header;
    return true;
  }
};
}
