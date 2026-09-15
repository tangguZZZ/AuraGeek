#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace aurageek::services {
// One deadline for headers AND body. Writer must itself be nonblocking:
// positive = bytes accepted, zero = retryable backpressure, negative = fatal.
class PortalWriteBudget {
 public:
  static constexpr uint32_t kTimeoutMs=3000;
  explicit PortalWriteBudget(uint32_t started):started_(started){}
  template<class Write,class Clock,class Pause>
  bool writeAll(const uint8_t* data,size_t size,Write write,Clock clock,Pause pause){
    if(size&&!data)return false;
    size_t sent=0;
    while(sent<size){
      if(uint32_t(clock()-started_)>=kTimeoutMs)return false;
      const size_t chunk=std::min<size_t>(1024,size-sent);
      const auto n=write(data+sent,chunk);
      if(n<0||size_t(n)>chunk)return false;
      sent+=size_t(n);
      if(sent<size)pause();
    }
    return true;
  }
 private:
  uint32_t started_;
};
}
