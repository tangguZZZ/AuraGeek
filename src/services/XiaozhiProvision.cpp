#include "services/XiaozhiProvision.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_random.h>
#include <ctime>

namespace aurageek::services {
namespace {
struct Response { String body; bool overflow = false; };
esp_err_t receive(esp_http_client_event_t* event) {
  if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
  auto* response = static_cast<Response*>(event->user_data);
  if (event->data_len < 0 || response->body.length() + event->data_len > 8192) {
    response->overflow = true; return ESP_FAIL;
  }
  if (!response->body.concat(static_cast<const char*>(event->data), event->data_len)) {
    response->overflow = true; return ESP_FAIL;
  }
  return ESP_OK;
}
String clientId(Preferences& prefs) {
  String id = prefs.isKey("client-id") ? prefs.getString("client-id", "") : String();
  if (id.length() == 36) return id;
  uint8_t bytes[16]; esp_fill_random(bytes, sizeof(bytes));
  bytes[6] = (bytes[6] & 15) | 64; bytes[8] = (bytes[8] & 63) | 128;
  char text[37];
  snprintf(text, sizeof(text), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
  if (!prefs.putString("client-id", text)) return String();
  return String(text);
}
}
bool XiaozhiProvision::request() {
  if (WiFi.status() != WL_CONNECTED || time(nullptr) < 1700000000) {
    Serial.println("[AI] Wi-Fi and NTP required before certificate-verified provisioning"); return false;
  }
  bool expected = false;
  if (!busy_.compare_exchange_strong(expected, true)) return false;
  if (xTaskCreate(taskEntry, "xiaozhi-setup", 12288, this, 2, nullptr) != pdPASS) { busy_ = false; return false; }
  return true;
}
void XiaozhiProvision::taskEntry(void* self) {
  auto* service = static_cast<XiaozhiProvision*>(self);
  service->check(); service->busy_ = false; vTaskDelete(nullptr);
}
void XiaozhiProvision::check() {
  status_ = -1;
  Preferences prefs;
  if (!prefs.begin("aura-ai", false)) { Serial.println("[AI] NVS unavailable"); return; }
  String id = clientId(prefs);
  if (id.isEmpty()) { Serial.println("[AI] client identity save failed"); return; }
  String mac = WiFi.macAddress(); mac.toLowerCase();
  JsonDocument info;
  info["version"] = 2; info["language"] = "zh-CN";
  info["flash_size"] = ESP.getFlashChipSize();
  info["mac_address"] = mac; info["uuid"] = id;
  info["chip_model_name"] = "esp32s3";
  info["application"]["name"] = "AuraGeek";
  info["application"]["version"] = "0.1.0";
  info["application"]["idf_version"] = ESP.getSdkVersion();
  info["board"]["type"] = "aurageek-esp32s3";
  info["board"]["name"] = "AuraGeek";
  info["board"]["mac"] = mac;
  String body; serializeJson(info, body);
  Response response;
  esp_http_client_config_t config{};
  config.url = "https://api.tenclass.net/xiaozhi/ota/";
  config.method = HTTP_METHOD_POST;
  config.timeout_ms = 10000;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.disable_auto_redirect = true;
  config.event_handler = receive; config.user_data = &response;
  auto client = esp_http_client_init(&config);
  if (!client) { Serial.println("[AI] HTTP initialization failed"); return; }
  esp_http_client_set_header(client, "Activation-Version", "1");
  esp_http_client_set_header(client, "Device-Id", mac.c_str());
  esp_http_client_set_header(client, "Client-Id", id.c_str());
  esp_http_client_set_header(client, "User-Agent", "AuraGeek/0.1.0 (ESP32-S3)");
  esp_http_client_set_header(client, "Accept-Language", "zh-CN");
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, body.c_str(), body.length());
  auto result = esp_http_client_perform(client);
  const int code = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);
  Serial.printf("[AI] provision HTTP=%d transport=%s oversized=%u\n", code, esp_err_to_name(result), unsigned(response.overflow));
  if (result != ESP_OK || code != 200 || response.overflow) return;
  JsonDocument doc;
  if (deserializeJson(doc, response.body)) { Serial.println("[AI] invalid provision JSON"); return; }
  // Do not log full responses, endpoint query strings, or bearer tokens.
  String activation = doc["activation"]["code"] | "";
  if (!activation.isEmpty()) {
    bool digits = activation.length() <= 12;
    for (char c : activation) digits = digits && c >= '0' && c <= '9';
    if (digits) Serial.printf("[AI] activation code: %s (enter at xiaozhi.me, then ai provision again)\n", activation.c_str());
    else Serial.println("[AI] activation required; unexpected code format");
    status_ = 1;
    return;
  }
  String url = doc["websocket"]["url"] | "";
  String token = doc["websocket"]["token"] | "";
  int version = doc["websocket"]["version"] | 1;
  if (!url.startsWith("wss://") || url.length() > 1024 || token.length() > 2048 || version < 1 || version > 3) {
    Serial.println("[AI] no supported secure WebSocket configuration; chat not ready"); return;
  }
  JsonDocument session;
  session["url"] = url; session["token"] = token; session["version"] = version;
  String stored; serializeJson(session, stored);
  if (prefs.putString("ws-config", stored) != stored.length()) { Serial.println("[AI] configuration save failed"); return; }
  status_ = 2;
  Serial.println("[AI] secure endpoint provisioned; open Agent and click to talk; firmware offers ignored");
}
void XiaozhiProvision::printStatus() const {
  Serial.printf("[AI] provision_state=%d busy=%u (0=unchecked 1=activation 2=configured -1=error)\n", status_.load(), unsigned(busy_.load()));
}
}
