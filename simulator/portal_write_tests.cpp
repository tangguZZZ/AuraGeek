#include "services/PortalWriteBudget.h"
#include <cassert>
#include <cstdio>
#include <vector>
using aurageek::services::PortalWriteBudget;
int main(){
  const uint8_t data[]={1,2,3,4,5};uint32_t now=100;unsigned calls=0;
  auto clock=[&](){return now;};auto pause=[&](){++now;};
  PortalWriteBudget stalled(now);
  assert(!stalled.writeAll(data,5,[&](const uint8_t*,size_t){++calls;return 0;},clock,pause));
  assert(now==3100&&calls==3000); // No unbounded retry on backpressure.
  now=0;calls=0;PortalWriteBudget partial(now);std::vector<uint8_t> received;
  auto write=[&](const uint8_t* p,size_t n){++calls;assert(n<=1024);if(calls==1)return 0;received.push_back(*p);return 1;};
  assert(partial.writeAll(data,5,write,clock,pause));
  assert(received==std::vector<uint8_t>(data,data+5)); // Partial writes never duplicate bytes.
  now=0;calls=0;received.clear();PortalWriteBudget page(now);
  std::vector<uint8_t> large(20000);for(size_t i=0;i<large.size();++i)large[i]=uint8_t(i);
  assert(page.writeAll(large.data(),large.size(),[&](const uint8_t* p,size_t n){
    ++calls;assert(n<=1024);if(calls%4==0)return 0;
    const size_t accepted=std::min<size_t>(113,n);received.insert(received.end(),p,p+accepted);return int(accepted);
  },clock,pause));
  assert(received==large); // Full-page-sized binary data, split across many writes.
  now=0;PortalWriteBudget shared(now);
  assert(shared.writeAll(data,1,[](const uint8_t*,size_t){return 1;},clock,pause));
  now=2999;calls=0;
  assert(!shared.writeAll(data,5,[&](const uint8_t*,size_t){++calls;return 1;},clock,pause));
  assert(calls==1&&now==3000); // The body does not restart the header's budget.
  now=0xfffffff0U;PortalWriteBudget wrap(now);
  assert(!wrap.writeAll(data,5,[](const uint8_t*,size_t){return 0;},clock,pause));
  assert(now==uint32_t(0xfffffff0U+3000U));
  now=0;PortalWriteBudget failure(now);
  assert(!failure.writeAll(data,5,[](const uint8_t*,size_t){return -1;},clock,pause));
  assert(!failure.writeAll(data,5,[](const uint8_t*,size_t){return 6;},clock,pause));
  assert(!failure.writeAll(nullptr,5,write,clock,pause));
  assert(failure.writeAll(nullptr,0,write,clock,pause));
  puts("Portal response writes: shared deadline, backpressure, partial writes, disconnect and timer wrap PASS");
}
