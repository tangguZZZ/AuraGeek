#pragma once

#include <lvgl.h>

#include "ui/AuraUi.h"

namespace aurageek {
namespace drivers {
class DisplayDriver;
}

namespace ui {

class UiService {
 public:
  bool begin(drivers::DisplayDriver& displayDriver);
  void process();

  void setNetwork(bool connected, bool connecting, int rssi);
  void setClock(bool valid, const char* timeText, const char* dateText);
  void setWeather(bool valid, const char* weatherText, int weatherCode);
  void setIndoorUnavailable();
  void setAiPending();
  AuraUi& screen() { return editorUi_; }

 private:
  static uint32_t tickMillis();
  static void log(lv_log_level_t level, const char* message);

  AuraUi editorUi_;
  bool ready_ = false;
};

}  // namespace ui
}  // namespace aurageek
