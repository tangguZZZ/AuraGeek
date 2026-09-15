#pragma once
#include "services/PortalConfig.h"
#include <Arduino.h>
#include <atomic>

namespace aurageek::services {
// Owns one short diagnostic session. Never opens audio or writes service config.
// PortalService has application lifetime; the worker must finish before exit.
class PortalAiProbe {
 public:
  enum class State {Idle,Clock,Connect,Hello,Passed,ClockFailed,ConnectFailed,Unauthorized,HelloFailed,MemoryFailed};
  bool start(const PortalConfig& config,String& error);
  bool active()const{auto s=state_.load();return s==State::Clock||s==State::Connect||s==State::Hello;}
  void json(JsonObject out)const;
 private:
  static void task(void* self);
  State run();
  std::atomic<State> state_{State::Idle};
  std::atomic<int> httpStatus_{0};
  String url_,token_,clientId_;
  unsigned version_=1,job_=0;
};
}
