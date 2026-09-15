#include "services/VoiceIdentity.h"
#include "drivers/VoicePacket.h"
#include "services/VoiceHello.h"
#include <cassert>
#include <cstdio>
using aurageek::services::VoiceIdentity;
using aurageek::drivers::VoicePacket;
int main(){
  std::string stored;unsigned writes=0,randomCalls=0;
  auto read=[&](){return stored;};
  auto write=[&](const std::string& s){++writes;stored=s;return true;};
  auto random=[&](auto& bytes){++randomCalls;for(size_t i=0;i<bytes.size();++i)bytes[i]=uint8_t(i);};
  auto id=VoiceIdentity::loadOrCreate(read,write,random);
  assert(id=="00010203-0405-4607-8809-0a0b0c0d0e0f"&&writes==1&&randomCalls==1);
  assert(VoiceIdentity::loadOrCreate(read,write,random)==id&&writes==1&&randomCalls==1);
  stored="malformed";assert(VoiceIdentity::loadOrCreate(read,write,random).empty()&&writes==1);
  stored.clear();assert(VoiceIdentity::loadOrCreate(read,[](auto&){return false;},random).empty());
  assert(VoiceIdentity::loadOrCreate(read,[](auto&){return true;},random).empty()); // Readback failed.
  assert(!VoiceIdentity::valid("00010203_0405-4607-8809-0a0b0c0d0e0f"));
  uint8_t packet[1516]{};
  for(unsigned version=1;version<=3;++version){
    const unsigned header=VoicePacket::headerSize(version);
    assert(VoicePacket::writeHeader(version,packet,sizeof(packet),1500));
    packet[header]=0x78;assert(VoicePacket::valid(version,packet,header+1500));
    assert(!VoicePacket::valid(version,packet,header));
    assert(!VoicePacket::writeHeader(version,packet,header+9,10));
    assert(!VoicePacket::writeHeader(version,packet,sizeof(packet),0));
    if(version>1){
      assert(!VoicePacket::valid(version,packet,header+1499));
      packet[0]=1;assert(!VoicePacket::valid(version,packet,header+1500));
    }
  }
  assert(!VoicePacket::writeHeader(0,packet,sizeof(packet),1));
  assert(!VoicePacket::writeHeader(4,packet,sizeof(packet),1));
  assert(!VoicePacket::valid(4,packet,1));
  assert(!VoicePacket::valid(1,nullptr,1));
  JsonDocument hello;aurageek::services::VoiceHello::request(hello,3);
  assert(hello["version"].as<unsigned>()==3&&hello["audio_params"]["channels"].as<unsigned>()==1);
  assert(hello["features"].isUnbound()); // Never advertise AEC/MCP that the diagnostic does not implement.
  auto compatible=[&](){return aurageek::services::VoiceHello::compatible(hello.as<JsonVariantConst>());};
  assert(!compatible());hello["session_id"]="session";assert(compatible());
  hello["audio_params"]["sample_rate"]=24000;assert(compatible());
  hello["audio_params"]["channels"]=2;assert(!compatible());
  hello["audio_params"]["channels"]=1;hello["audio_params"]["format"]="pcm";assert(!compatible());
  hello["audio_params"]["format"]="opus";hello["audio_params"]["frame_duration"]=20;assert(!compatible());
  hello["audio_params"]["frame_duration"]=60;hello["audio_params"]["sample_rate"]="24000";assert(!compatible());
  puts("PASS voice connection: independent stable identity, failed persistence, v1/v2/v3 audio framing and malformed packets");
}
