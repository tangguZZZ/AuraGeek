#pragma once
#include <cstdint>
namespace aurageek::drivers {
// 0 = digital mute; 100 = unity (0 dB). Never amplify or exceed int16 PCM.
// Squared taper gives more useful adjustment at quiet listening levels.
constexpr int volumeGainQ15(unsigned percent){
 const unsigned p=percent>100?100:percent;
 return int((p*p*32768U+5000U)/10000U);
}
inline int16_t applyVolumeSample(int16_t sample,int& gain,unsigned percent){
 const int target=volumeGainQ15(percent),delta=target-gain;
 gain+=delta>128?128:(delta< -128?-128:delta);
 return int16_t(int32_t(sample)*gain/32768);
}
}
