#pragma once
#include "NetworkClient.h"
// Isolated listener-result double. This does not simulate a real TCP stack.
class NetworkServer {
 public:
  inline static bool startSucceeds=true;
  inline static unsigned starts=0,ends=0,noDelayCalls=0;
  inline static IPAddress bound{0,0,0,0};
  inline static uint16_t port=0;
  inline static uint8_t backlog=0;
  NetworkServer(IPAddress ip,uint16_t p,uint8_t clients){bound=ip;port=p;backlog=clients;}
  ~NetworkServer(){end();}
  void begin(){++starts;listening_=startSucceeds;}
  operator bool(){return listening_;}
  void end(){++ends;listening_=false;}
  void setNoDelay(bool){++noDelayCalls;}
 private:
  bool listening_=false;
};
