#include "ui/UiService.h"

#include <Arduino.h>

#include "drivers/DisplayDriver.h"

namespace aurageek {
namespace ui {

bool UiService::begin(drivers::DisplayDriver& displayDriver) {
  displayDriver_=&displayDriver;
  if (!psramFound()) { Serial.println("[UI] N16R8 PSRAM required for UI and GIF"); return false; }
  lv_init();
  lv_tick_set_cb(tickMillis);
  lv_log_register_print_cb(log);

  if (displayDriver.attachToLvgl() == nullptr) {
    Serial.println("[UI] Failed to create LVGL display");
    return false;
  }

  if (!editorUi_.create()) {
    Serial.println("[UI] Failed to create AuraGeek screen");
    return false;
  }
  ready_ = true;

  setNetwork(false, true, -127);
  setClock(false, "--:--", "");
  setWeather(false, "", -1);
  setIndoorUnavailable();
  setAiPending();

  Serial.printf("[UI] AuraGeek UI ready on LVGL %u.%u.%u, 320x240 RGB565\n",
                lv_version_major(), lv_version_minor(), lv_version_patch());
  return true;
}

void UiService::process() {
  if (ready_) {
    displayDriver_->process();
    lv_timer_handler();
    displayDriver_->process();
  }
}

void UiService::setNetwork(bool connected, bool connecting, int rssi) {
  if (ready_) editorUi_.setNetwork(connected, connecting, rssi);
}

void UiService::setClock(bool valid, const char* timeText, const char* dateText) {
  if (ready_) editorUi_.setClock(valid, timeText, dateText);
}

void UiService::setWeather(bool valid, const char* weatherText, int weatherCode,
                           const float* hourlyTemperature, size_t hourlyCount) {
  if (ready_)
    editorUi_.setWeather(valid, weatherText, weatherCode, hourlyTemperature,
                         hourlyCount);
}

void UiService::setIndoorUnavailable() {
  if (ready_) editorUi_.setIndoorUnavailable();
}

void UiService::setIndoor(bool valid, float temperature, float humidity) {
  if (ready_) editorUi_.setIndoor(valid, temperature, humidity);
}

void UiService::setAiPending() {
  if (ready_) editorUi_.setAiPending();
}

uint32_t UiService::tickMillis() { return millis(); }

void UiService::log(lv_log_level_t level, const char* message) {
  (void)level;
  Serial.printf("[LVGL] %s\n", message);
}

}  // namespace ui
}  // namespace aurageek
