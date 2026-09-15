#pragma once

#include <Arduino.h>
#include <cstddef>

namespace aurageek {
namespace services {

class WeatherService {
 public:
  struct Snapshot {
    static constexpr size_t kHourlyCapacity = 24;
    bool valid = false;
    int weatherCode = -1;
    char text[32] = "";
    float hourlyTemperature[kHourlyCapacity]{};
    size_t hourlyCount = 0;
  };

  void begin();
  void configure(double latitude,double longitude){latitude_=latitude;longitude_=longitude;}
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
  double latitude_=22.5431,longitude_=114.0579;
};

}  // namespace services
}  // namespace aurageek
