#pragma once
#include <cstdint>
namespace aurageek::services {
struct PortalSubnets {
  static bool overlap(uint32_t a,uint32_t maskA,uint32_t b,uint32_t maskB){
    const uint32_t firstA=a&maskA,lastA=firstA|~maskA;
    const uint32_t firstB=b&maskB,lastB=firstB|~maskB;
    return firstA<=lastB&&firstB<=lastA;
  }
};
}
