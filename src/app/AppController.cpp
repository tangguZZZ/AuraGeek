#include "app/AppController.h"

#include <Arduino.h>
#include <cstring>

#include "config/Secrets.local.h"

namespace aurageek {
namespace app {

namespace {
const services::NetworkService::Credential kWifiAllowList[] = {
    {AURAGEEK_WIFI_PRIMARY_SSID, AURAGEEK_WIFI_PRIMARY_PASSWORD},
    {AURAGEEK_WIFI_BACKUP_SSID, AURAGEEK_WIFI_BACKUP_PASSWORD},
};
}

void AppController::begin() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("=== AuraGeek shared native LVGL UI ===");
  Serial.printf("Chip: %s, revision %u, cores %u\n", ESP.getChipModel(),
                ESP.getChipRevision(), ESP.getChipCores());
  Serial.printf("Flash: %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("PSRAM: %u bytes, detected: %s\n", ESP.getPsramSize(),
                psramFound() ? "yes" : "no");

  statusLed_.begin();
  encoder_.begin();
  Serial.println("[LED] GPIO48 WS2812 RMT effect ready");
  Serial.println("[INPUT] EC11 A=39 B=40 PUSH=21; KEY0=41; active-low");

  if (!displayDriver_.begin()) {
    Serial.println("[APP] TFT initialization failed");
    statusLed_.setState(drivers::Ws2812Driver::State::kError);
    return;
  }

  if (!uiService_.begin(displayDriver_)) {
    Serial.println("[APP] LVGL initialization failed");
    statusLed_.setState(drivers::Ws2812Driver::State::kError);
    return;
  }

  timeService_.begin();
  weatherService_.begin();
  stockService_.begin();
  if(!usbAudio_.begin())Serial.println("[APP] USB Audio initialization failed");
  networkService_.begin(kWifiAllowList,
                        sizeof(kWifiAllowList) / sizeof(kWifiAllowList[0]));

  ready_ = true;
  Serial.println("[APP] AuraGeek home and network services ready");
}

void AppController::process() {
  statusLed_.process();

  if (ready_) {
    processBenchCommands();
    usbAudio_.process();
    uiService_.screen().setSpectrum(usbAudio_.bands(),48,usbAudio_.active());
    auto input=encoder_.poll();
    if(input.rotation){Serial.printf("[INPUT] rotate=%d\n",input.rotation);uiService_.screen().rotate(input.rotation);}
    if(input.click){Serial.println("[INPUT] EC11 click");uiService_.screen().click();}
    if(input.back){Serial.println("[INPUT] back");uiService_.screen().back();}
    if(input.home){Serial.println("[INPUT] home");uiService_.screen().home();}
    networkService_.process();
    const bool networkConnected = networkService_.connected();
    timeService_.process(networkConnected);
    weatherService_.process(networkConnected);
    stockService_.process(networkConnected,timeService_.localEpoch());

    const uint32_t now = millis();
    if (now - lastUiUpdateMs_ >= 250U) {
      lastUiUpdateMs_ = now;
      uiService_.setNetwork(networkConnected, networkService_.connecting(),
                            networkService_.rssi());

      char timeText[8] = "--:--";
      char dateText[32] = "";
      const bool timeValid = timeService_.valid(networkConnected);
      if (timeValid) {
        timeService_.formatTime(timeText, sizeof(timeText));
        timeService_.formatDate(dateText, sizeof(dateText));
      }
      uiService_.setClock(timeValid, timeText, dateText);

      const services::WeatherService::Snapshot weather =
          weatherService_.snapshot(networkConnected);
      uiService_.setWeather(weather.valid, weather.text, weather.weatherCode);
      uiService_.screen().setEpoch(timeService_.localEpoch());
      auto stock=stockService_.snapshot(uiService_.screen().stockIndex(),uiService_.screen().stockRange());
      uiService_.screen().setStock(stock);
    }

    uiService_.process();
    delay(5);
    return;
  }

  const uint32_t now = millis();
  if (now - lastErrorLogMs_ >= 2000U) {
    lastErrorLogMs_ = now;
    Serial.println("[APP] Initialization incomplete");
  }
  delay(10);
}

void AppController::processBenchCommands(){
#if AURAGEEK_BENCH_COMMANDS
  while(Serial.available()){
    char c=char(Serial.read());if(c=='\r')continue;
    if(c=='\n'){
      command_[commandLength_]=0;auto& ui=uiService_.screen();using Page=ui::AuraUi::Page;
      if(!strcmp(command_,"ui home"))ui.show(Page::Home);
      else if(!strcmp(command_,"ui menu"))ui.show(Page::Menu);
      else if(!strcmp(command_,"ui stocks"))ui.show(Page::Stocks);
      else if(!strcmp(command_,"ui spectrum"))ui.show(Page::Spectrum);
      else if(!strcmp(command_,"ui agent"))ui.show(Page::Agent);
      else if(!strcmp(command_,"ui next"))ui.click();
      else if(!strcmp(command_,"stock status")){
        for(unsigned i=0;i<2;++i){auto s=stockService_.snapshot(i,4);Serial.printf("[BENCH] %s bars=%u %lu..%lu last=%.3f source=%s\n",s.ticker,unsigned(s.total),(unsigned long)s.firstDate,(unsigned long)s.lastDate,s.last,s.status);}
        Serial.printf("[BENCH] heap=%u PSRAM_free=%u uptime_ms=%lu\n",ESP.getFreeHeap(),ESP.getFreePsram(),(unsigned long)millis());
      }
      else if(!strncmp(command_,"range ",6)&&commandLength_==7&&command_[6]>='0'&&command_[6]<='4')ui.setStockRange(command_[6]-'0');
      else {Serial.println("[BENCH] commands: ui home/menu/stocks/spectrum/agent/next; range 0..4; stock status");commandLength_=0;continue;}
      Serial.printf("[BENCH] page=%u range=%u\n",unsigned(ui.page()),ui.stockRange());commandLength_=0;
    }else if(commandLength_<sizeof(command_)-1)command_[commandLength_++]=c;
    else commandLength_=0;
  }
#endif
}

}  // namespace app
}  // namespace aurageek
