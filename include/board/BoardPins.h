#pragma once

#include <Arduino.h>

#include "board/TftSetup.h"

namespace aurageek {
namespace board {

// ST7789 ZJY240S10Z0TG11, confirmed by the project GPIO baseline.
static constexpr uint8_t kTftChipSelect = TFT_CS;
static constexpr uint8_t kTftMosi = TFT_MOSI;
static constexpr uint8_t kTftClock = TFT_SCLK;
static constexpr uint8_t kTftDataCommand = TFT_DC;
static constexpr uint8_t kTftReset = TFT_RST;
static constexpr uint8_t kTftBacklight = 18;
static constexpr uint8_t kOnboardWs2812 = 48;

// 2.4-inch TFT + EC11 module input header. The module already provides 10 kOhm
// pull-ups and RC filtering; all four inputs are active low. GPIO41 is a safe,
// otherwise-unused input for the module's independent KEY0 push button.
static constexpr uint8_t kEncoderA = 39;
static constexpr uint8_t kEncoderB = 40;
static constexpr uint8_t kEncoderPush = 21;
static constexpr uint8_t kAuxButton = 41;

// The panel itself is 240 x 320. TFT_eSPI rotation 1 selects the 320 x 240
// landscape direction confirmed after rotating the first landscape build by
// another 180 degrees.
static constexpr uint16_t kDisplayWidth = TFT_HEIGHT;
static constexpr uint16_t kDisplayHeight = TFT_WIDTH;
static constexpr uint16_t kLvglDrawBufferLines = 20;

}  // namespace board
}  // namespace aurageek
