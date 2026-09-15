#pragma once
#include "services/PortalConfigStore.h"
#include "services/PortalHttp.h"
#include "services/NetworkCredentials.h"
#include "services/PortalScanState.h"
#include "services/PortalAiProbe.h"
#include "services/PortalConnectState.h"
#include <NetworkUdp.h>
#include <memory>
#include <atomic>

namespace aurageek::services {
class PortalService {
 public:
  static bool consumeBootRequest();
  static bool requestBoot();
  bool begin(PortalConfigStore& store);
  void process();
  const String& ssid()const{return apSsid_;}
  const String& password()const{return apPassword_;}
  const String& address()const{return address_;}
  const String& connectionStatus()const{return connectionStatus_;}
  bool exitRequested()const{return exitAt_&&int32_t(millis()-exitAt_)>=0;}
  bool canExit()const{return !aiProbe_.active();}
 private:
  void route(const PortalHttp::Request& request);
  void dns();
  void reply(int status,bool ok,const char* message);
  void connectNetwork(const NetworkCredentials& network,bool reuseSaved);
  PortalConfigStore* store_=nullptr;
  std::unique_ptr<PortalHttp> http_;
  NetworkUDP dns_;
  IPAddress ip_{192,168,4,1};
  String apSsid_,apPassword_,address_,token_,connectionStatus_="idle";
  NetworkCredentials candidate_,savedNetwork_;
  bool hasSavedNetwork_=false,reuseSavedNetwork_=false;
  PortalConnectState connect_;
  PortalScanState scan_;
  PortalAiProbe aiProbe_;
  uint32_t exitAt_=0,lastActivity_=0,lastHeap_=0;
  unsigned revision_=0;
  std::atomic<unsigned> disconnectReason_{0};
  std::atomic<bool> gotIp_{false};
};
}
