#pragma once

#include <TFT_eSPI.h>
#include <lvgl.h>
#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace aurageek {
namespace drivers {

class DisplayDriver {
 public:
  bool begin();
  lv_display_t* attachToLvgl();
  void process();
  void printStats();
  void setBrightness(unsigned percent);

 private:
  static void flush(lv_display_t* display, const lv_area_t* area,
                    uint8_t* pixelMap);
  static void waitFlush(lv_display_t* display);
  bool finishTransfer(bool wait=true);
  static bool transferDone(esp_lcd_panel_io_handle_t,esp_lcd_panel_io_event_data_t*,void*);

  TFT_eSPI tft_;
  esp_lcd_panel_io_handle_t io_=nullptr;
  SemaphoreHandle_t completion_=nullptr;
  volatile uint32_t finishedUs_=0;
  lv_display_t* lvglDisplay_ = nullptr;
  uint16_t* shadow_ = nullptr;
  bool pending_ = false, lastChunk_ = false;
  uint32_t bytes_ = 0, chunks_ = 0, updates_ = 0, skipped_ = 0;
  uint32_t startedUs_ = 0, transferUs_ = 0, statsStartedMs_ = 0;
};

}  // namespace drivers
}  // namespace aurageek
