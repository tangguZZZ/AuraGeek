#pragma once
#include <cstdint>

namespace aurageek::services {
// Wait for the old association to disappear before starting a new test. An old
// WL_CONNECTED observation must never authorize saving untested credentials.
class PortalConnectState {
 public:
  enum class Action {Wait,Begin,Passed,TimedOut};
  bool active()const{return phase_!=Phase::Idle;}
  void start(uint32_t now){phase_=Phase::Disconnecting;started_=now;}
  Action step(bool connected,bool freshGotIp,bool sameSsid,uint32_t now){
    if(phase_==Phase::Disconnecting){
      if(!connected){phase_=Phase::Connecting;started_=now;return Action::Begin;}
      if(uint32_t(now-started_)>=3000){phase_=Phase::Idle;return Action::TimedOut;}
    }else if(phase_==Phase::Connecting){
      if(connected&&freshGotIp&&sameSsid){phase_=Phase::Idle;return Action::Passed;}
      if(uint32_t(now-started_)>=20000){phase_=Phase::Idle;return Action::TimedOut;}
    }
    return Action::Wait;
  }
 private:
  enum class Phase {Idle,Disconnecting,Connecting};
  Phase phase_=Phase::Idle;
  uint32_t started_=0;
};
}
