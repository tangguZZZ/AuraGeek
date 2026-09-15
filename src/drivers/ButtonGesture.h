#pragma once
#include <cstdint>

namespace aurageek { namespace drivers {
// Platform-independent state machine; caller supplies regularly sampled levels.
class ButtonGesture {
 public:
  enum class Action : uint8_t { None, Single, Double, Long };
  explicit ButtonGesture(uint32_t holdMs=1200,uint32_t doubleMs=300)
      : holdMs_(holdMs),doubleMs_(doubleMs) {}
  Action sample(bool pressed,uint32_t now) {
    Action result=Action::None;
    if(pressed!=raw_){raw_=pressed;changed_=now;}
    // A second raw down inside the window may still be undergoing debounce.
    // Do not expire the first click while that candidate is being validated.
    if(waiting_&&now-up_>=doubleMs_&&(!raw_||changed_-up_>=doubleMs_)){
      waiting_=false;result=Action::Single;
    }
    if(now-changed_>=20&&stable_!=raw_){
      stable_=raw_;
      if(stable_){
        down_=now;longSent_=false;
        second_=waiting_&&changed_-up_<doubleMs_;
        if(second_)waiting_=false;
      }else if(!longSent_){
        if(second_){second_=false;result=Action::Double;}
        else if(doubleMs_==0)result=Action::Single;
        else {waiting_=true;up_=now;}
      }
    }
    if(stable_&&!longSent_&&now-down_>=holdMs_){
      longSent_=true;waiting_=second_=false;result=Action::Long;
    }
    return result;
  }
 private:
  const uint32_t holdMs_,doubleMs_;
  bool raw_=false,stable_=false,longSent_=false,waiting_=false,second_=false;
  uint32_t changed_=0,down_=0,up_=0;
};
}}
