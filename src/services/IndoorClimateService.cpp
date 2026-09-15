#include "services/IndoorClimateService.h"

#include <Wire.h>
#include <cmath>

#include "board/BoardPins.h"

namespace aurageek {
namespace services {
namespace {

void scanI2cBus() {
  unsigned found = 0;
  Serial.print("[AHT20] I2C scan:");
  for (uint8_t address = 8; address < 120; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf(" 0x%02X", address);
      ++found;
    }
  }
  if (found == 0) Serial.print(" no devices");
  Serial.println();
}

}  // namespace

void IndoorClimateService::begin() {
  Wire.begin(board::kAht20Sda, board::kAht20Scl);
  Wire.setClock(100000);

  if (xTaskCreate(taskEntry, "aht20_task", 4096, this, 1, &worker_) != pdPASS) {
    worker_ = nullptr;
    Serial.println("[AHT20] Failed to create sampling task");
    return;
  }
  Serial.printf("[AHT20] Monitoring SDA=%u SCL=%u at 100 kHz\n",
                board::kAht20Sda, board::kAht20Scl);
}

IndoorClimateService::Snapshot IndoorClimateService::snapshot() const {
  Snapshot copy;
  portENTER_CRITICAL(&lock_);
  copy = snapshot_;
  portEXIT_CRITICAL(&lock_);
  return copy;
}

void IndoorClimateService::taskEntry(void* context) {
  static_cast<IndoorClimateService*>(context)->workerLoop();
}

void IndoorClimateService::workerLoop() {
  bool sensorReady = false;
  bool loggedValid = false;
  unsigned probeAttempts = 0;
  for (;;) {
    if (!sensorReady) {
      sensorReady = sensor_.begin(&Wire);
      if (!sensorReady) {
        if (probeAttempts % 6 == 0) {
          Serial.printf("[AHT20] No response on SDA=%u SCL=%u; retrying\n",
                        board::kAht20Sda, board::kAht20Scl);
          scanI2cBus();
        }
        ++probeAttempts;
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }
      Serial.println("[AHT20] Ready at address 0x38; sample interval=2000 ms");
      probeAttempts = 0;
    }

    sensors_event_t humidityEvent{};
    sensors_event_t temperatureEvent{};
    Snapshot next;
    const bool readOk = sensor_.getEvent(&humidityEvent, &temperatureEvent);
    if (readOk) {
      next.temperature = temperatureEvent.temperature;
      next.humidity = humidityEvent.relative_humidity;
      next.valid = std::isfinite(next.temperature) &&
                   std::isfinite(next.humidity) && next.temperature >= -40.0f &&
                   next.temperature <= 85.0f && next.humidity >= 0.0f &&
                   next.humidity <= 100.0f;
    }

    portENTER_CRITICAL(&lock_);
    snapshot_ = next;
    portEXIT_CRITICAL(&lock_);

    if (next.valid && !loggedValid) {
      Serial.printf("[AHT20] First sample: %.1f C, %.1f %%RH\n",
                    next.temperature, next.humidity);
    } else if (!next.valid && loggedValid) {
      Serial.println("[AHT20] Sample invalid; UI returned to placeholders");
    }
    loggedValid = next.valid;
    if (!readOk) sensorReady = false;
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

}  // namespace services
}  // namespace aurageek
