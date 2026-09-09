#include "services/WeatherService.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

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
  char url[256];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code&timezone=Asia%%2FShanghai&forecast_days=1",
           config::kWeatherLatitude, config::kWeatherLongitude);

  WiFiClientSecure client;
  client.setInsecure();  // MVP only; replace with a pinned CA before release.
  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(4000);

  Snapshot next;
  if (http.begin(client, url)) {
    const int status = http.GET();
    if (status == HTTP_CODE_OK) {
      JsonDocument document;
      const DeserializationError error = deserializeJson(document, http.getString());
      if (!error && document["current"]["temperature_2m"].is<float>() &&
          document["current"]["weather_code"].is<int>()) {
        const float temperature = document["current"]["temperature_2m"].as<float>();
        const int code = document["current"]["weather_code"].as<int>();
        snprintf(next.text, sizeof(next.text), "%s  %.0f℃", conditionForCode(code),
                 temperature);
        next.weatherCode = code;
        next.valid = true;
        Serial.printf("[WEATHER] Updated: %s\n", next.text);
      } else {
        Serial.printf("[WEATHER] JSON error: %s\n", error.c_str());
      }
    } else {
      Serial.printf("[WEATHER] HTTP status %d\n", status);
    }
    http.end();
  } else {
    Serial.println("[WEATHER] HTTPS begin failed");
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
