#pragma once
#include <algorithm>
#include <cmath>
namespace aurageek { namespace ui {
// Direct eased tracking, not an inertial spring. The minimum closing speed
// removes the long subpixel tail after the final detent. No velocity survives
// a new target; selection remains the exact sum of encoder detents.
inline void stepMenuMotion(float& position,float target,float& velocity,float dt){
 const float remaining=target-position;
 velocity=0;
 if(std::fabs(remaining)<.001f){position=target;return;}
 dt=std::max(0.f,std::min(dt,.1f));
 const float distance=std::fabs(remaining);
 const float step=std::max(distance*(1.f-std::exp(-24.f*dt)),3.f*dt);
 if(step>=distance){position=target;return;}
 position+=std::copysign(step,remaining);
}
}}
