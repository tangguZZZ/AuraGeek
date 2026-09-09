#pragma once

#include <TFT_eSPI.h>
#include <lvgl.h>

namespace aurageek {
namespace drivers {

class DisplayDriver {
 public:
  bool begin();
  lv_display_t* attachToLvgl();

 private:
  static void flush(lv_display_t* display, const lv_area_t* area,
                    uint8_t* pixelMap);

  TFT_eSPI tft_;
  lv_display_t* lvglDisplay_ = nullptr;
};

}  // namespace drivers
}  // namespace aurageek
