#pragma once

#include "drivers/DisplayDriver.h"
#include "drivers/EncoderDriver.h"
#include "drivers/UsbAudioDriver.h"
#include "drivers/VoiceAudio.h"
#include "drivers/Ws2812Driver.h"
#include "services/NetworkService.h"
#include "services/PortalService.h"
#include "services/PortalExitGuard.h"
#include "services/XiaozhiProvision.h"
#include "services/IndoorClimateService.h"
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
  services::PortalConfigStore portalConfig_;
  services::PortalService portal_;
  bool portalMode_=false;
  bool resetFailed_=false;
  services::PortalExitGuard portalExitGuard_;
  uint32_t networkFailureSince_=0;
  lv_obj_t* portalStatus_=nullptr;
  void enterPortal();
  drivers::DisplayDriver displayDriver_;
  drivers::EncoderDriver encoder_;
  drivers::UsbAudioDriver usbAudio_;
  drivers::VoiceAudio voiceAudio_;
  drivers::Ws2812Driver statusLed_;
  services::NetworkService networkService_;
  services::XiaozhiProvision xiaozhiProvision_;
  services::IndoorClimateService indoorClimateService_;
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
  ui::UserSettings storedSettings_{};
  unsigned appliedBrightness_=101;
  unsigned appliedSpectrum_=2;
};

}  // namespace app
}  // namespace aurageek
