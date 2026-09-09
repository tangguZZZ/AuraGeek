#pragma once
#include <Arduino.h>
#include "board/BoardPins.h"
namespace aurageek {namespace drivers {
class EncoderDriver {
 public:
  struct Event{int rotation=0;bool click=false,back=false,home=false;};
  void begin(){
    pinMode(board::kEncoderA,INPUT_PULLUP);pinMode(board::kEncoderB,INPUT_PULLUP);
    pinMode(board::kEncoderPush,INPUT_PULLUP);pinMode(board::kAuxButton,INPUT_PULLUP);
    previous_=(digitalRead(board::kEncoderA)<<1)|digitalRead(board::kEncoderB);
  }
  Event poll(){
    Event e;uint32_t now=millis();int state=(digitalRead(board::kEncoderA)<<1)|digitalRead(board::kEncoderB);
    static const int8_t transition[]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
    accumulated_+=transition[(previous_<<2)|state];previous_=state;
    if(state==3&&accumulated_){if(abs(accumulated_)>=4)e.rotation=accumulated_>0?1:-1;accumulated_=0;}
    bool raw=digitalRead(board::kEncoderPush)==LOW;if(raw!=raw_){raw_=raw;changed_=now;}
    if(now-changed_>=20&&stable_!=raw_){stable_=raw_;if(stable_){down_=now;longSent_=false;}else if(!longSent_){if(waiting_&&now-up_<300){e.back=true;waiting_=false;}else{waiting_=true;up_=now;}}}
    if(stable_&&!longSent_&&now-down_>=1200){longSent_=true;waiting_=false;e.home=true;}
    if(waiting_&&!stable_&&now-up_>=300){waiting_=false;e.click=true;}
    bool auxRaw=digitalRead(board::kAuxButton)==LOW;
    if(auxRaw!=auxRaw_){auxRaw_=auxRaw;auxChanged_=now;}
    if(now-auxChanged_>=20&&auxStable_!=auxRaw_){
      auxStable_=auxRaw_;
      if(auxStable_){auxDown_=now;auxLongSent_=false;}
      else if(!auxLongSent_)e.back=true;
    }
    if(auxStable_&&!auxLongSent_&&now-auxDown_>=800){auxLongSent_=true;e.home=true;}
    return e;
  }
 private:
  int previous_=3,accumulated_=0;
  bool raw_=false,stable_=false,longSent_=false,waiting_=false;
  bool auxRaw_=false,auxStable_=false,auxLongSent_=false;
  uint32_t changed_=0,down_=0,up_=0,auxChanged_=0,auxDown_=0;
};
}}
