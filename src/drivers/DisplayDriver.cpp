#include "drivers/DisplayDriver.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <esp_lcd_io_spi.h>
#include <driver/spi_master.h>

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

  shadow_ = static_cast<uint16_t*>(heap_caps_calloc(
      board::kDisplayWidth * board::kDisplayHeight, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  completion_=xSemaphoreCreateBinary();
  if (!shadow_ || !completion_) {
    Serial.println("[TFT] DMA/shadow allocation failed");
    return false;
  }
  // TFT_eSPI supplies ONLY the already validated panel initialization. Release
  // Arduino's SPI peripheral before esp_lcd takes exclusive ownership of SPI3.
  // Do not mix TFT direct-register transactions with IDF DMA transactions.
  tft_.getSPIinstance().end();
  spi_bus_config_t bus{};
  bus.mosi_io_num=board::kTftMosi;bus.miso_io_num=-1;bus.sclk_io_num=board::kTftClock;
  bus.quadwp_io_num=bus.quadhd_io_num=-1;
  bus.data4_io_num=bus.data5_io_num=bus.data6_io_num=bus.data7_io_num=-1;
  bus.max_transfer_sz=kDrawBufferBytes;
  ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST,&bus,SPI_DMA_CH_AUTO));
  esp_lcd_panel_io_spi_config_t config{};
  config.cs_gpio_num=board::kTftChipSelect;config.dc_gpio_num=board::kTftDataCommand;
  config.spi_mode=0;config.pclk_hz=SPI_FREQUENCY;config.trans_queue_depth=2;
  config.lcd_cmd_bits=8;config.lcd_param_bits=8;
  config.on_color_trans_done=transferDone;config.user_ctx=this;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST,&config,&io_));
  statsStartedMs_ = millis();
  Serial.printf("[TFT] esp_lcd SPI3 DMA, 2x40 SRAM lines, RGB565_SWAPPED, configured=%luHz, TE=not-wired\n", (unsigned long)SPI_FREQUENCY);

  if(!ledcAttach(board::kTftBacklight,5000,8)){Serial.println("[TFT] backlight PWM failed");return false;}
  setBrightness(100);
  Serial.println("[TFT] Controller ready; backlight enabled");
  return true;
}

void DisplayDriver::setBrightness(unsigned percent){ledcWrite(board::kTftBacklight,(std::max(10U,std::min(100U,percent))*255U+50U)/100U);}

lv_display_t* DisplayDriver::attachToLvgl() {
  if (lvglDisplay_ != nullptr) {
    return lvglDisplay_;
  }

  lvglDisplay_ =
      lv_display_create(board::kDisplayWidth, board::kDisplayHeight);
  if (lvglDisplay_ == nullptr) {
    return nullptr;
  }

  lv_display_set_color_format(lvglDisplay_, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_driver_data(lvglDisplay_, this);
  lv_display_set_flush_cb(lvglDisplay_, flush);
  lv_display_set_flush_wait_cb(lvglDisplay_, waitFlush);
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

  self->finishTransfer();
  self->lastChunk_ = lv_display_flush_is_last(display);
  auto* pixels = reinterpret_cast<uint16_t*>(pixelMap);
  // Lossless dirty bounding box within each LVGL strip. Never discard a changed pixel.
  int left=width, right=-1, top=height, bottom=-1;
  for (unsigned y=0;y<height;++y) {
    auto* previous=self->shadow_+(area->y1+y)*board::kDisplayWidth+area->x1;
    for (unsigned x=0;x<width;++x) if(previous[x]!=pixels[y*width+x]) {
      left=std::min(left,int(x));right=std::max(right,int(x));
      top=std::min(top,int(y));bottom=std::max(bottom,int(y));
    }
  }
  if(right<left) {
    ++self->skipped_;
    if(self->lastChunk_)++self->updates_;
    lv_display_flush_ready(display);
    return;
  }
  const unsigned w=right-left+1, h=bottom-top+1;
  for(unsigned y=0;y<h;++y) {
    auto* source=pixels+(y+top)*width+left;
    memcpy(self->shadow_+(area->y1+top+y)*board::kDisplayWidth+area->x1+left,source,w*2);
    memmove(pixels+y*w,source,w*2); // compact in-place; LVGL PARTIAL does not reuse old pixels
  }
  unsigned x1=area->x1+left,x2=x1+w-1,y1=area->y1+top,y2=y1+h-1;
  uint8_t columns[]={uint8_t(x1>>8),uint8_t(x1),uint8_t(x2>>8),uint8_t(x2)};
  uint8_t rows[]={uint8_t(y1>>8),uint8_t(y1),uint8_t(y2>>8),uint8_t(y2)};
  ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(self->io_,0x2a,columns,4));
  ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(self->io_,0x2b,rows,4));
  self->startedUs_=micros();
  self->pending_=true;
  ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(self->io_,0x2c,pixels,w*h*2));
  self->bytes_+=w*h*2;
  ++self->chunks_;
  // Return immediately: LVGL can render the other buffer while DMA reads this one.
}

bool DisplayDriver::transferDone(esp_lcd_panel_io_handle_t,esp_lcd_panel_io_event_data_t*,void* context){
  auto* self=static_cast<DisplayDriver*>(context);self->finishedUs_=micros();
  BaseType_t woken=pdFALSE;xSemaphoreGiveFromISR(self->completion_,&woken);return woken==pdTRUE;
}

bool DisplayDriver::finishTransfer(bool wait) {
  if(!pending_)return false;
  if(xSemaphoreTake(completion_,wait?pdMS_TO_TICKS(1000):0)!=pdTRUE){
    if(wait)ESP_ERROR_CHECK(ESP_ERR_TIMEOUT);
    return false;
  }
  transferUs_+=finishedUs_-startedUs_;
  pending_=false;
  if(lastChunk_)++updates_;
  return true;
}

void DisplayDriver::waitFlush(lv_display_t* display) {
  static_cast<DisplayDriver*>(lv_display_get_driver_data(display))->finishTransfer();
}

void DisplayDriver::process() {
  if(finishTransfer(false)) {
    lv_display_flush_ready(lvglDisplay_);
  }
}

void DisplayDriver::printStats() {
  Serial.printf("[DISPLAY] elapsed_ms=%lu batches=%lu dma_chunks=%lu bytes=%lu skipped=%lu dma_us=%lu heap=%u largest_dma=%u\n",
    (unsigned long)(millis()-statsStartedMs_),(unsigned long)updates_,(unsigned long)chunks_,
    (unsigned long)bytes_,(unsigned long)skipped_,(unsigned long)transferUs_,ESP.getFreeHeap(),
    unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA)));
  statsStartedMs_=millis();updates_=chunks_=bytes_=skipped_=transferUs_=0;
}

}  // namespace drivers
}  // namespace aurageek
