#include "drivers/DisplayDriver.h"

#include <Arduino.h>

#include "board/BoardPins.h"

namespace {

static_assert(LV_COLOR_DEPTH == 16,
              "AuraGeek ST7789 flush expects LVGL RGB565 color data");

constexpr size_t kBytesPerPixel = 2;
constexpr size_t kDrawBufferBytes =
    aurageek::board::kDisplayWidth *
    aurageek::board::kLvglDrawBufferLines * kBytesPerPixel;

alignas(4) uint8_t drawBufferA[kDrawBufferBytes];
alignas(4) uint8_t drawBufferB[kDrawBufferBytes];

}  // namespace

namespace aurageek {
namespace drivers {

bool DisplayDriver::begin() {
  // Keep the panel dark until the controller has been initialized and cleared.
  pinMode(board::kTftBacklight, OUTPUT);
  digitalWrite(board::kTftBacklight, LOW);

  Serial.println("[TFT] Starting HSPI: SCLK=12 MOSI=11 MISO=none CS=10");
  Serial.println("[TFT] Initializing ST7789 controller");
  tft_.init();
  // Rotation 1 is the opposite 320 x 240 landscape orientation from rotation
  // 3. It applies the additional 180-degree turn requested after the first
  // landscape bench check. BGR, inversion and RGB565 byte swap stay unchanged.
  tft_.setRotation(1);
  tft_.fillScreen(TFT_BLACK);

  digitalWrite(board::kTftBacklight, HIGH);
  Serial.println("[TFT] Controller ready; backlight enabled");
  return true;
}

lv_display_t* DisplayDriver::attachToLvgl() {
  if (lvglDisplay_ != nullptr) {
    return lvglDisplay_;
  }

  lvglDisplay_ =
      lv_display_create(board::kDisplayWidth, board::kDisplayHeight);
  if (lvglDisplay_ == nullptr) {
    return nullptr;
  }

  lv_display_set_color_format(lvglDisplay_, LV_COLOR_FORMAT_RGB565);
  lv_display_set_driver_data(lvglDisplay_, this);
  lv_display_set_flush_cb(lvglDisplay_, flush);
  lv_display_set_buffers(lvglDisplay_, drawBufferA, drawBufferB,
                         sizeof(drawBufferA), LV_DISPLAY_RENDER_MODE_PARTIAL);
  return lvglDisplay_;
}

void DisplayDriver::flush(lv_display_t* display, const lv_area_t* area,
                          uint8_t* pixelMap) {
  DisplayDriver* self = static_cast<DisplayDriver*>(
      lv_display_get_driver_data(display));

  const uint32_t width = static_cast<uint32_t>(lv_area_get_width(area));
  const uint32_t height = static_cast<uint32_t>(lv_area_get_height(area));

  self->tft_.startWrite();
  self->tft_.setAddrWindow(area->x1, area->y1, width, height);
  self->tft_.pushColors(reinterpret_cast<uint16_t*>(pixelMap), width * height,
                        true);
  self->tft_.endWrite();

  lv_display_flush_ready(display);
}

}  // namespace drivers
}  // namespace aurageek
