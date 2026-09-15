#pragma once

#include <Adafruit_AHTX0.h>
#include <Arduino.h>

namespace aurageek {
namespace services {

class IndoorClimateService {
 public:
  struct Snapshot {
    bool valid = false;
    float temperature = 0.0f;
    float humidity = 0.0f;
  };

  void begin();
  Snapshot snapshot() const;

 private:
  static void taskEntry(void* context);
  void workerLoop();

  Adafruit_AHTX0 sensor_;
  TaskHandle_t worker_ = nullptr;
  mutable portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
  Snapshot snapshot_;
};

}  // namespace services
}  // namespace aurageek
