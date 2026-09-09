#pragma once

#include "drivers/DisplayDriver.h"
#include "drivers/EncoderDriver.h"
#include "drivers/UsbAudioDriver.h"
#include "drivers/Ws2812Driver.h"
#include "services/NetworkService.h"
#include "services/TimeService.h"
#include "services/WeatherService.h"
#include "services/StockService.h"
#include "ui/UiService.h"

namespace aurageek {
namespace app {

class AppController {
 public:
  void begin();
  void process();

 private:
  drivers::DisplayDriver displayDriver_;
  drivers::EncoderDriver encoder_;
  drivers::UsbAudioDriver usbAudio_;
  drivers::Ws2812Driver statusLed_;
  services::NetworkService networkService_;
  services::TimeService timeService_;
  services::WeatherService weatherService_;
  services::StockService stockService_;
  ui::UiService uiService_;
  bool ready_ = false;
  uint32_t lastErrorLogMs_ = 0;
  uint32_t lastUiUpdateMs_ = 0;
  void processBenchCommands();
  char command_[32]{};
  unsigned commandLength_=0;
};

}  // namespace app
}  // namespace aurageek
