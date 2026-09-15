#pragma once
#include <cstdint>
namespace aurageek::services {
class PortalScanState {
 public:
  enum class State { Idle, Running, Ready, Failed };
  void start(uint32_t now){state_=State::Running;since_=now;count_=0;}
  void reset(){state_=State::Idle;count_=0;}
  bool running()const{return state_==State::Running;}
  State state()const{return state_;}
  int count()const{return count_;}
  // Driver results: -1 running, -2 failed, >=0 AP count.
  // Return true when retained driver memory must be discarded.
  bool update(int result,uint32_t now){
    if(running()){
      if(result>=0){state_=State::Ready;count_=result;since_=now;}
      else if(result==-2||uint32_t(now-since_)>=20000){state_=State::Failed;since_=now;return true;}
    }else if((state_==State::Ready||state_==State::Failed)&&uint32_t(now-since_)>=30000){reset();return true;}
    return false;
  }
 private:
  State state_=State::Idle;
  uint32_t since_=0;
  int count_=0;
};
}
