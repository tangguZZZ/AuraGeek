#pragma once
#include <cstdint>
#include <string>
using String=std::string;
struct IPAddress {
  uint8_t octets[4];
  IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d):octets{a,b,c,d}{}
};
class NetworkClient {};
