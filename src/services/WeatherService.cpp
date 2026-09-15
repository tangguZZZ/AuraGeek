#include "services/WeatherService.h"

#include <ArduinoJson.h>
#include "services/SecureHttpGet.h"
#include <ctime>
#include <cmath>

#include "config/AppConfig.h"

namespace aurageek {
namespace services {

void WeatherService::begin() {
  const BaseType_t result =
      xTaskCreate(taskEntry, "weather_task", 8192, this, 1, &worker_);
  if (result != pdPASS) {
    worker_ = nullptr;
    Serial.println("[WEATHER] Failed to create background task");
  }
}

void WeatherService::process(bool networkConnected) {
  // TLS date checks require UTC. Treat the first clock-ready loop as connect,
  // so a slow NTP sync does not defer the initial weather update for 15 minutes.
  networkConnected = networkConnected && time(nullptr)>=1704067200;
  const uint32_t now = millis();
  const bool justConnected = networkConnected && !lastNetworkConnected_;
  const bool intervalElapsed = now - lastRequestMs_ >= config::kWeatherUpdateIntervalMs;
  lastNetworkConnected_ = networkConnected;

  if (networkConnected && worker_ != nullptr && (justConnected || intervalElapsed)) {
    lastRequestMs_ = now;
    xTaskNotifyGive(worker_);
  }
}

WeatherService::Snapshot WeatherService::snapshot(bool networkConnected) const {
  Snapshot copy;
  portENTER_CRITICAL(&lock_);
  copy = snapshot_;
  portEXIT_CRITICAL(&lock_);
  if (!networkConnected) {
    copy.valid = false;
    copy.text[0] = '\0';
  }
  return copy;
}

void WeatherService::taskEntry(void* context) {
  static_cast<WeatherService*>(context)->workerLoop();
}

void WeatherService::workerLoop() {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    fetch();
  }
}

void WeatherService::fetch() {
  char url[384];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code&hourly=temperature_2m"
           "&past_hours=23&forecast_hours=1&timezone=Asia%%2FShanghai",
           latitude_, longitude_);

  Snapshot next;SecureHttpResponse response;
  if (secureHttpGet(url,32768,12000,response)) {
      JsonDocument document;
      const DeserializationError error = deserializeJson(document, static_cast<const char*>(response.body.get()), response.length, DeserializationOption::NestingLimit(8));
      if (!error && document["current"]["temperature_2m"].is<float>() &&
          document["current"]["weather_code"].is<int>() &&
          std::isfinite(document["current"]["temperature_2m"].as<float>())) {
        const float temperature = document["current"]["temperature_2m"].as<float>();
        const int code = document["current"]["weather_code"].as<int>();
        snprintf(next.text, sizeof(next.text), "%s  %.0f℃", conditionForCode(code),
                 temperature);
        next.weatherCode = code;
        next.valid = true;
        JsonArray hourly = document["hourly"]["temperature_2m"].as<JsonArray>();
        const size_t available = hourly.size();
        const size_t first = available > Snapshot::kHourlyCapacity
                                 ? available - Snapshot::kHourlyCapacity
                                 : 0;
        for (size_t i = first; i < available &&
                                   next.hourlyCount < Snapshot::kHourlyCapacity;
             ++i) {
          if (!hourly[i].is<float>()) continue;
          const float sample = hourly[i].as<float>();
          if (std::isfinite(sample)) {
            next.hourlyTemperature[next.hourlyCount++] = sample;
          }
        }
        Serial.printf("[WEATHER] Updated: %s, recent-hours=%u\n", next.text,
                      static_cast<unsigned>(next.hourlyCount));
      } else {
        Serial.printf("[WEATHER] JSON error: %s\n", error.c_str());
      }
  } else {
    Serial.printf("[WEATHER] HTTP=%d transport=%s\n", response.status, response.error);
  }

  portENTER_CRITICAL(&lock_);
  snapshot_ = next;
  portEXIT_CRITICAL(&lock_);
}

const char* WeatherService::conditionForCode(int code) {
  switch (code) {
    case 0: return "晴";
    case 1: return "少云";
    case 2: return "多云";
    case 3: return "阴";
    case 45:
    case 48: return "雾";
    case 51:
    case 53:
    case 55: return "毛毛雨";
    case 56:
    case 57: return "冻毛毛雨";
    case 61: return "小雨";
    case 63: return "中雨";
    case 65: return "大雨";
    case 66:
    case 67: return "冻雨";
    case 71: return "小雪";
    case 73: return "中雪";
    case 75: return "大雪";
    case 77: return "小雪";
    case 80: return "小阵雨";
    case 81: return "中阵雨";
    case 82: return "大阵雨";
    case 85: return "小阵雪";
    case 86: return "大阵雪";
    case 95: return "雷雨";
    case 96:
    case 99: return "雷暴";
    default: return "未知";
  }
}

}  // namespace services
}  // namespace aurageek
