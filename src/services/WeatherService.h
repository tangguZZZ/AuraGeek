#pragma once

#include <Arduino.h>

namespace aurageek {
namespace services {

class WeatherService {
 public:
  struct Snapshot {
    bool valid = false;
    int weatherCode = -1;
    char text[32] = "";
  };

  void begin();
  void process(bool networkConnected);
  Snapshot snapshot(bool networkConnected) const;

 private:
  static void taskEntry(void* context);
  void workerLoop();
  void fetch();
  static const char* conditionForCode(int code);

  TaskHandle_t worker_ = nullptr;
  mutable portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
  Snapshot snapshot_;
  bool lastNetworkConnected_ = false;
  uint32_t lastRequestMs_ = 0;
};

}  // namespace services
}  // namespace aurageek
