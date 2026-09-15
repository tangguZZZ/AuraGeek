#pragma once
#include <cstdint>

namespace aurageek::drivers {
// GPIO state is (A << 1) | B. A full Gray-code cycle is one detent.
// Backtracking on either contact cancels partial progress rather than emitting
// an opposite UI step. Invalid two-bit jumps resynchronize without a step.
class QuadratureDecoder {
 public:
  void reset(uint8_t ab){previous_=anchor_=ab&3u;progress_=0;invalid_=0;recovering_=false;}
  int sample(uint8_t ab){
    ab&=3u;
    if(ab==previous_)return 0;
    if((ab^previous_)==3u){previous_=ab;progress_=0;recovering_=true;++invalid_;return 0;}
    // This orientation matches the previous A-rising/B-low positive direction.
    static constexpr int8_t delta[16]={0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
    const int direction=delta[(previous_<<2)|ab];previous_=ab;
    if(recovering_){if(ab==anchor_){recovering_=false;progress_=0;}return 0;}
    progress_+=direction;
    if(ab!=anchor_)return 0;
    const int step=progress_==4?1:progress_==-4?-1:0;
    progress_=0;return step;
  }
  uint32_t invalid()const{return invalid_;}
  uint8_t state()const{return previous_;}
 private:
  uint8_t previous_=3,anchor_=3;
  int8_t progress_=0;
  bool recovering_=false;
  uint32_t invalid_=0;
};
}
