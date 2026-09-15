#include "services/PortalHttp.h"
#include <cassert>
#include <cstdio>
#include <memory>
using aurageek::services::PortalHttp;
int main(){
  PortalHttp http(IPAddress(192,168,4,1));
  assert(NetworkServer::bound.octets[0]==192&&NetworkServer::bound.octets[1]==168);
  assert(NetworkServer::bound.octets[2]==4&&NetworkServer::bound.octets[3]==1);
  assert(NetworkServer::port==80&&NetworkServer::backlog==2);
  NetworkServer::startSucceeds=false;
  auto owned=std::make_shared<int>(7);std::weak_ptr<int> weak=owned;
  assert(!http.begin([owned](const PortalHttp::Request&){}));
  owned.reset();assert(weak.expired()); // Failed startup must not retain the callback.
  assert(NetworkServer::starts==1&&NetworkServer::ends==1&&NetworkServer::noDelayCalls==0);
  NetworkServer::startSucceeds=true;
  assert(http.begin([](const PortalHttp::Request&){}));
  assert(NetworkServer::starts==2&&NetworkServer::noDelayCalls==1);
  puts("Portal listener: explicit AP binding, failed-listener cleanup and successful startup PASS");
}
