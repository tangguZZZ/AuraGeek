#pragma once

// TFT_eSPI is configured at compile time. PlatformIO force-includes this
// project-owned file for application code and library translation units.
#define USER_SETUP_LOADED 1

#define ST7789_DRIVER 1
#define TFT_WIDTH 240
#define TFT_HEIGHT 320

#define TFT_MISO -1
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS 10
#define TFT_DC 13
#define TFT_RST 14

// This panel expects the ST7789 MADCTL BGR bit. Without it, red and blue are
// exchanged: yellow artwork (for example the sun icon) appears blue while
// green remains mostly unchanged.
#define TFT_RGB_ORDER TFT_BGR

// This ZJY240S10Z0TG11 panel needs normal (non-inverted) colors. TFT_eSPI's
// generic ST7789 init sequence sends INVON, so explicitly override it after
// initialization. Without this, dark navy becomes pale and green becomes
// magenta on the physical panel.
#define TFT_INVERSION_OFF 1

// TFT_eSPI 2.5.43 plus the current Arduino-ESP32 core maps the default FSPI
// software index (0) incorrectly in its ESP32-S3 direct-register path.
// HSPI selects the second general-purpose SPI controller and keeps the same
// GPIO matrix pin assignment above.
#define USE_HSPI_PORT 1

#define SPI_FREQUENCY 20000000
