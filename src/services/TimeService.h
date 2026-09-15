#pragma once

#include <NTPClient.h>
#include <WiFiUdp.h>

namespace aurageek {
namespace services {

class TimeService {
 public:
  TimeService();

  void begin();
  void configure(int offsetMinutes){offsetSeconds_=offsetMinutes*60;client_.setTimeOffset(offsetSeconds_);}
  void process(bool networkConnected);
  bool valid(bool networkConnected) const;
  void formatTime(char* output, size_t outputSize) const;
  void formatDate(char* output, size_t outputSize) const;
  uint32_t localEpoch() const { return synchronized_ ? client_.getEpochTime() : 0; }
  uint32_t utcEpoch() const { return synchronized_ ? uint32_t(int64_t(client_.getEpochTime())-offsetSeconds_) : 0; }

 private:
  WiFiUDP udp_;
  NTPClient client_;
  bool clientStarted_ = false;
  bool synchronized_ = false;
  int32_t offsetSeconds_=480*60;
};

}  // namespace services
}  // namespace aurageek
