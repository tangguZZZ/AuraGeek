#pragma once

#include <Arduino.h>
#include <driver/spi_common.h>

#include "board/TftSetup.h"

namespace aurageek {
namespace board {

// Current 2.4-inch ST7789 TFT + EC11 integrated module (SPI baseline).
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

static constexpr uint8_t kAht20Sda = 8;
static constexpr uint8_t kAht20Scl = 9;

// Current prototype: separate RX/TX I2S buses, matching the Wiki wiring.
static constexpr uint8_t kMicWs = 4;
static constexpr uint8_t kMicSck = 5;
static constexpr uint8_t kMicData = 6;
static constexpr uint8_t kSpeakerBclk = 7;
static constexpr uint8_t kSpeakerLrc = 15;
static constexpr uint8_t kSpeakerData = 16;
static constexpr uint8_t kSpeakerEnable = 17;

// Reserved ONLY: MicroSD SPI adapter has not arrived; no SD driver/init yet.
// Dedicated SPI2 via GPIO matrix; the display keeps exclusive SPI3 ownership.
// Adapter VCC = 5V; all ESP-facing logic must remain 3.3V. No external WS2812.
static constexpr spi_host_device_t kSdSpiHost = SPI2_HOST;
static constexpr uint8_t kSdClock = 42;
static constexpr uint8_t kSdMosi = 1;
static constexpr uint8_t kSdMiso = 2;
static constexpr uint8_t kSdChipSelect = 47;

// Include board-internal USB/UART signals in compile-time allocation checks.
constexpr bool gpioAllocationIsUnique() {
  const uint8_t pins[] = {
    kTftChipSelect,kTftMosi,kTftClock,kTftDataCommand,kTftReset,kTftBacklight,
    kOnboardWs2812,kEncoderA,kEncoderB,kEncoderPush,kAuxButton,kAht20Sda,kAht20Scl,
    kMicWs,kMicSck,kMicData,kSpeakerBclk,kSpeakerLrc,kSpeakerData,kSpeakerEnable,
    19,20,43,44,kSdClock,kSdMosi,kSdMiso,kSdChipSelect
  };
  for(size_t i=0;i<sizeof(pins)/sizeof(pins[0]);++i)
    for(size_t j=i+1;j<sizeof(pins)/sizeof(pins[0]);++j)
      if(pins[i]==pins[j])return false;
  return true;
}
static_assert(gpioAllocationIsUnique(),"Board GPIO allocation conflict (including reserved SD)");

// The panel itself is 240 x 320. TFT_eSPI rotation 1 selects the 320 x 240
// landscape direction confirmed after rotating the first landscape build by
// another 180 degrees.
static constexpr uint16_t kDisplayWidth = TFT_HEIGHT;
static constexpr uint16_t kDisplayHeight = TFT_WIDTH;
static constexpr uint16_t kLvglDrawBufferLines = 40;

}  // namespace board
}  // namespace aurageek
