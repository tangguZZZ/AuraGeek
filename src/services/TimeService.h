#pragma once

#include <NTPClient.h>
#include <WiFiUdp.h>

namespace aurageek {
namespace services {

class TimeService {
 public:
  TimeService();

  void begin();
  void process(bool networkConnected);
  bool valid(bool networkConnected) const;
  void formatTime(char* output, size_t outputSize) const;
  void formatDate(char* output, size_t outputSize) const;
  uint32_t localEpoch() const { return synchronized_ ? client_.getEpochTime() : 0; }

 private:
  WiFiUDP udp_;
  NTPClient client_;
  bool clientStarted_ = false;
  bool synchronized_ = false;
};

}  // namespace services
}  // namespace aurageek
