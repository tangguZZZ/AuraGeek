#include "app/AppController.h"

#include <Arduino.h>
#include <cstring>
#include <Preferences.h>
#include <esp_system.h>
#include "services/FactoryReset.h"

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
  using Reset=services::FactoryReset<Preferences>;
  const auto resetResult=Reset::resume();
  resetFailed_=resetResult==Reset::Result::Failed;
  if(resetFailed_)Serial.println("[RESET] Failed; pending reset retained, normal services stopped. Reboot to retry.");
  if(resetResult==Reset::Result::Completed)Serial.println("[RESET] Local defaults restored; official binding and stock request guards preserved.");
  portalMode_=services::PortalService::consumeBootRequest();
  services::NetworkCredentials savedNetwork;
  if(Reset::userNetworkOnly()&&!savedNetwork.load())portalMode_=true;
  if(esp_reset_reason()!=ESP_RST_SW&&digitalRead(board::kAuxButton)==LOW){
    uint32_t began=millis();while(digitalRead(board::kAuxButton)==LOW&&millis()-began<3000)delay(10);
    if(millis()-began>=3000)portalMode_=true;
  }
  if(!portalConfig_.load())Serial.printf("[PORTAL] saved preferences rejected: %s\n",portalConfig_.error().c_str());
  const auto& web=portalConfig_.value();
  uiService_.screen().configureHome(web.background,web.foreground,web.accent,web.city.c_str());
  uiService_.screen().configureStocks(web.stocks);
  stockService_.configure(web.stocks);
  voiceAudio_.configure(web);
  timeService_.configure(web.utcOffsetMinutes);
  weatherService_.configure(web.latitude,web.longitude);
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

  if(resetFailed_){
    auto* message=lv_label_create(lv_screen_active());
    lv_label_set_text(message,"Reset incomplete\nNormal services stopped\nPlease restart to retry");
    lv_obj_center(message);ready_=true;return;
  }
  {
    Preferences prefs;ui::UserSettings loaded;bool migrated=false;uint8_t bytes[5]{};
    if(prefs.begin("aura-ui",true)){
      if(prefs.getBytesLength("settings")==sizeof(bytes)&&prefs.getBytes("settings",bytes,sizeof(bytes))==sizeof(bytes)&&ui::UserSettings::decode(bytes,sizeof(bytes),loaded,migrated))storedSettings_=loaded;
      prefs.end();
    }
    uiService_.screen().settings=storedSettings_;uiService_.screen().applySettings();
    appliedSpectrum_=storedSettings_.spectrum;
    displayDriver_.setBrightness(storedSettings_.brightness);appliedBrightness_=storedSettings_.brightness;
    voiceAudio_.setVolume(storedSettings_.volume);
    Serial.printf("[SETTINGS] loaded brightness=%u volume=%u transition=%u spectrum=%u version=%u migrated=%u\n",storedSettings_.brightness,storedSettings_.volume,storedSettings_.transition,storedSettings_.spectrum,storedSettings_.version,unsigned(migrated));
  }
  if(portalMode_){
    if(!portal_.begin(portalConfig_)){portalMode_=false;Serial.println("[PORTAL] start failed; normal mode retained");}
    else{
      auto* page=lv_obj_create(nullptr);lv_obj_set_style_bg_color(page,lv_color_hex(0x090909),0);lv_obj_remove_flag(page,LV_OBJ_FLAG_SCROLLABLE);
      auto* title=lv_label_create(page);lv_label_set_text(title,"AuraGeek / WEB CONFIG");lv_obj_set_style_text_font(title,&lv_font_montserrat_16,0);lv_obj_set_style_text_color(title,lv_color_hex(0xf2f2f2),0);lv_obj_set_pos(title,8,6);
      auto* details=lv_label_create(page);String message="1. Join Wi-Fi:\n"+portal_.ssid()+"\n\nPassword: "+portal_.password()+"\n\n2. Open: "+portal_.address()+"\n\nKEY0: exit / restore device";lv_label_set_text(details,message.c_str());lv_obj_set_width(details,294);lv_obj_set_style_text_font(details,&lv_font_montserrat_12,0);lv_obj_set_style_text_color(details,lv_color_hex(0xc6c6c6),0);lv_obj_set_pos(details,8,38);
      portalStatus_=lv_label_create(page);lv_obj_set_style_text_font(portalStatus_,&lv_font_montserrat_10,0);lv_obj_set_style_text_color(portalStatus_,lv_color_hex(0xdfff00),0);lv_obj_set_pos(portalStatus_,8,199);lv_label_set_text(portalStatus_,"READY / CHAT PAUSED");lv_screen_load(page);
      ready_=true;return;
    }
  }
  timeService_.begin();
  indoorClimateService_.begin();
  weatherService_.begin();
  stockService_.begin();
  if(!voiceAudio_.begin())Serial.println("[APP] Voice audio worker creation failed");
  usbAudio_.attachSpeaker(voiceAudio_);
  if(!usbAudio_.begin())Serial.println("[APP] USB Audio initialization failed");
  networkService_.begin(kWifiAllowList,
                        sizeof(kWifiAllowList) / sizeof(kWifiAllowList[0]));

  ready_ = true;
  Serial.println("[APP] AuraGeek home and network services ready");
}

void AppController::process() {
  statusLed_.process();
  if(resetFailed_){if(ready_)uiService_.process();delay(10);return;}

  if(ready_&&portalMode_){processBenchCommands();portal_.process();uiService_.process();auto input=encoder_.poll();
    if(portalStatus_&&strcmp(lv_label_get_text(portalStatus_),portal_.connectionStatus().c_str()))lv_label_set_text(portalStatus_,portal_.connectionStatus().c_str());
    const bool released=digitalRead(board::kAuxButton)==HIGH&&digitalRead(board::kEncoderPush)==HIGH;
    const bool freshGesture=portalExitGuard_.accepts(released,millis(),input.sampledAt);
    if(((input.back||input.home)&&freshGesture)||portal_.exitRequested()){
      if(portal_.canExit())ESP.restart();
      else if(portalStatus_)lv_label_set_text(portalStatus_,"AI test running. Retry exit shortly.");
    }delay(1);return;
  }

  if (ready_) {
    auto& settingsScreen=uiService_.screen();
    if(!settingsScreen.settings.valid()){settingsScreen.settings=storedSettings_;settingsScreen.settingsSaveResult(false);}
    if(appliedBrightness_!=settingsScreen.settings.brightness){appliedBrightness_=settingsScreen.settings.brightness;displayDriver_.setBrightness(appliedBrightness_);}
    if(voiceAudio_.volume()!=settingsScreen.settings.volume)voiceAudio_.setVolume(settingsScreen.settings.volume);
    if(appliedSpectrum_!=settingsScreen.settings.spectrum){appliedSpectrum_=settingsScreen.settings.spectrum;settingsScreen.applySettings();}
    if(settingsScreen.takeSettingsCommit()){
      Preferences prefs;bool saved=false;
      if(settingsScreen.settings==storedSettings_)saved=true;
      else if(prefs.begin("aura-ui",false)){
        const auto& wanted=settingsScreen.settings;ui::UserSettings verify;
        saved=prefs.putBytes("settings",&wanted,sizeof(wanted))==sizeof(wanted);
        saved=saved&&prefs.getBytes("settings",&verify,sizeof(verify))==sizeof(verify)&&verify.valid()&&verify==wanted;
        prefs.end();
      }
      if(saved)storedSettings_=settingsScreen.settings;
      else settingsScreen.settings=storedSettings_;
      settingsScreen.settingsSaveResult(saved);
      Serial.printf("[SETTINGS] save=%s brightness=%u volume=%u transition=%u spectrum=%u\n",saved?"OK":"FAILED",settingsScreen.settings.brightness,settingsScreen.settings.volume,settingsScreen.settings.transition,settingsScreen.settings.spectrum);
    }
    processBenchCommands();
    if(settingsScreen.takePortalRequest())enterPortal();
    usbAudio_.process();
    uiService_.screen().setSpectrum(usbAudio_.bands(),48,usbAudio_.active());
    auto input=encoder_.poll();
    if(voiceAudio_.takeWakeRequest()){
      if(networkService_.connected()){uiService_.screen().show(ui::AuraUi::Page::Agent);voiceAudio_.toggleChat(true);}
      else Serial.println("[WAKE] offline; reconnect Wi-Fi before cloud conversation");
    }
    if(input.rotation){auto& screen=uiService_.screen();screen.rotate(input.rotation);Serial.printf("[INPUT] rotate=%d page=%u range=%u selection=%d edge_age_us=%lu\n",input.rotation,unsigned(screen.page()),screen.stockRange(),screen.selection(),(unsigned long)input.rotationAgeUs);}
    if(input.click){Serial.println("[INPUT] EC11 click");if(uiService_.screen().page()==ui::AuraUi::Page::Agent)voiceAudio_.toggleChat();else uiService_.screen().click();}
    if(input.doubleClick){Serial.println("[INPUT] EC11 double click");uiService_.screen().doubleClick();}
    if(input.back){Serial.println("[INPUT] back");uiService_.screen().back();}
    if(input.home){Serial.println("[INPUT] home");uiService_.screen().home();}
    if(uiService_.screen().page()!=ui::AuraUi::Page::Agent)voiceAudio_.cancelChat();
    using ChatState=drivers::VoiceAudio::ChatState;
    auto chatState=voiceAudio_.chatState();
    uiService_.screen().setAgentState(unsigned(chatState));
    uiService_.screen().setAgentEmotion(voiceAudio_.emotion());
    auto ledState=drivers::Ws2812Driver::State::kAmbient;
    if(chatState==ChatState::Connecting || chatState==ChatState::Thinking)ledState=drivers::Ws2812Driver::State::kThinking;
    if(chatState==ChatState::Listening)ledState=drivers::Ws2812Driver::State::kListening;
    if(chatState==ChatState::Speaking)ledState=drivers::Ws2812Driver::State::kSpeaking;
    if(chatState==ChatState::Error)ledState=drivers::Ws2812Driver::State::kError;
    if(statusLed_.state()!=ledState)statusLed_.setState(ledState);
    if(input.click||input.doubleClick||input.back||input.home){auto& ui=uiService_.screen();Serial.printf("[INPUT RESULT] page=%u picker=%u age_ms=%lu sample_gap_max_ms=%lu queue_dropped=%lu\n",unsigned(ui.page()),unsigned(ui.spectrumPickerOpen()),(unsigned long)(millis()-input.sampledAt),(unsigned long)input.maxSampleGap,(unsigned long)input.dropped);}
    networkService_.process();
    if(networkService_.connected())networkFailureSince_=0;
    else{if(!networkFailureSince_)networkFailureSince_=millis();if(millis()-networkFailureSince_>60000)enterPortal();}
    const bool networkConnected = networkService_.connected();
    timeService_.process(networkConnected);
    weatherService_.process(networkConnected);
    stockService_.process(networkConnected,timeService_.utcEpoch());

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
      uiService_.setWeather(weather.valid, weather.text, weather.weatherCode,
                            weather.hourlyTemperature, weather.hourlyCount);
      const services::IndoorClimateService::Snapshot indoor =
          indoorClimateService_.snapshot();
      uiService_.setIndoor(indoor.valid, indoor.temperature, indoor.humidity);
      uiService_.screen().setEpoch(timeService_.localEpoch());
      auto stock=stockService_.snapshot(uiService_.screen().stockIndex(),uiService_.screen().stockRange());
      uiService_.screen().setStock(stock);
    }

    // Range changes only rebuild a local snapshot; never wait for the 250 ms status tick.
    // This also handles range button events emitted by the previous LVGL iteration.
    if(uiService_.screen().stockRangePending()){
      auto& screen=uiService_.screen();
      screen.setStock(stockService_.snapshot(screen.stockIndex(),screen.stockRange()));
    }
    uiService_.process();
    delay(1);
    return;
  }

  const uint32_t now = millis();
  if (now - lastErrorLogMs_ >= 2000U) {
    lastErrorLogMs_ = now;
    Serial.println("[APP] Initialization incomplete");
  }
  delay(10);
}

void AppController::enterPortal(){
  voiceAudio_.cancelChat();
  if(services::PortalService::requestBoot()){Serial.println("[PORTAL] restarting into isolated configuration mode");delay(50);ESP.restart();}
  else Serial.println("[PORTAL] request save failed; staying in normal mode");
}

void AppController::processBenchCommands(){
#if AURAGEEK_BENCH_COMMANDS
  while(Serial.available()){
    char c=char(Serial.read());if(c=='\r')continue;
    if(c=='\n'){
      command_[commandLength_]=0;auto& ui=uiService_.screen();using Page=ui::AuraUi::Page;
      if(portalMode_){
        if(!strcmp(command_,"portal exit")){if(portal_.canExit())ESP.restart();else Serial.println("[PORTAL] AI test running; wait for completion before exit");}
        else Serial.printf("[PORTAL] mode=setup url=%s connection=%s; only portal exit is accepted\n",portal_.address().c_str(),portal_.connectionStatus().c_str());
        commandLength_=0;continue;
      }
      if(!strcmp(command_,"portal open"))enterPortal();
      else if(!strcmp(command_,"portal status")){const auto& c=portalConfig_.value();if(portalConfig_.error().length())Serial.printf("[PORTAL] load_error=%s\n",portalConfig_.error().c_str());Serial.printf("[PORTAL] mode=normal colors=%06lx/%06lx/%06lx utc=%d wake=%u greeting=%u continuous=%u silence=%u idle=%u max=%u service=%s\n",(unsigned long)c.background,(unsigned long)c.foreground,(unsigned long)c.accent,c.utcOffsetMinutes,c.wakeEnabled,c.wakeGreeting,c.continuous,c.silenceMs,c.noSpeechMs,c.maxSpeechSeconds,c.customService?"custom":"official");}
      else if(!strcmp(command_,"ui home"))ui.show(Page::Home);
      else if(!strcmp(command_,"ui menu"))ui.show(Page::Menu);
      else if(!strcmp(command_,"ui stocks"))ui.show(Page::Stocks);
      else if(!strcmp(command_,"ui spectrum"))ui.show(Page::Spectrum);
      else if(!strcmp(command_,"ui agent"))ui.show(Page::Agent);
      else if(!strcmp(command_,"ui settings"))ui.show(Page::Settings);
      else if(!strcmp(command_,"audio status"))voiceAudio_.printStatus();
      else if(!strcmp(command_,"ai status")){xiaozhiProvision_.printStatus();voiceAudio_.printStatus();}
      else if(!strcmp(command_,"ai start")){ui.show(Page::Agent);voiceAudio_.toggleChat();}
      else if(!strcmp(command_,"ai wake test")){Serial.println("[WAKE] bench greeting request; acoustic detection not exercised");ui.show(Page::Agent);voiceAudio_.toggleChat(true);}
      else if(!strcmp(command_,"ai stop"))voiceAudio_.cancelChat();
      else if(!strcmp(command_,"ai wake off"))voiceAudio_.setWakeEnabled(false);
      else if(!strcmp(command_,"ai wake on"))voiceAudio_.setWakeEnabled(true);
      else if(!strcmp(command_,"ai wake retry")){Serial.println("[WAKE] explicit single initialization retry requested");voiceAudio_.retryWake();}
      else if(!strcmp(command_,"ai provision"))Serial.printf("[AI] request=%s\n",xiaozhiProvision_.request()?"accepted":"not ready/busy");
      else if(!strcmp(command_,"audio stop"))voiceAudio_.stop();
      else if(!strcmp(command_,"audio usb"))voiceAudio_.enableUsb();
      else if(!strcmp(command_,"audio level low"))ui.settings.volume=25;
      else if(!strcmp(command_,"audio level normal"))ui.settings.volume=35;
      else if(!strcmp(command_,"audio level medium"))ui.settings.volume=50;
      else if(!strcmp(command_,"audio level full"))ui.settings.volume=100;
      else if(!strcmp(command_,"audio tone") || !strcmp(command_,"audio record") || !strcmp(command_,"audio play")){
        auto action=!strcmp(command_,"audio tone")?drivers::VoiceAudio::Command::Tone:(!strcmp(command_,"audio record")?drivers::VoiceAudio::Command::Record:drivers::VoiceAudio::Command::Play);
        Serial.printf("[VOICE] request=%s\n",voiceAudio_.request(action)?"accepted":"busy/not ready");
      }
      else if(!strcmp(command_,"ui reboot")){Serial.println("[BENCH] restarting for persistence verification");Serial.flush();ESP.restart();return;}
      else if(!strcmp(command_,"settings status"))Serial.printf("[SETTINGS] current brightness=%u volume=%u transition=%u spectrum=%u applied_brightness=%u applied_volume=%u stored_volume=%u version=%u\n",ui.settings.brightness,ui.settings.volume,ui.settings.transition,ui.settings.spectrum,appliedBrightness_,voiceAudio_.volume(),storedSettings_.volume,ui.settings.version);
      else if(!strcmp(command_,"ui next"))ui.click();
      else if(!strcmp(command_,"ui double"))ui.doubleClick();
      else if(!strcmp(command_,"ui back"))ui.back();
      else if(!strcmp(command_,"ui rotate 1"))ui.rotate(1);
      else if(!strcmp(command_,"ui rotate -1"))ui.rotate(-1);
      else if(!strcmp(command_,"display stats")){displayDriver_.printStats();Serial.printf("[SPECTRUM] last_render_ms=%lu\n",(unsigned long)ui.spectrumRenderMs());}
      else if(!strcmp(command_,"weather status")){auto weather=weatherService_.snapshot(networkService_.connected());Serial.printf("[WEATHER STATUS] valid=%u code=%d hours=%u text=%s\n",unsigned(weather.valid),weather.weatherCode,unsigned(weather.hourlyCount),weather.text);}
      else if(!strcmp(command_,"encoder status"))encoder_.printStatus();
      else if(!strcmp(command_,"ui motion"))Serial.printf("[MOTION] selection=%d settled=%u range=%u pending=%u\n",ui.selection(),unsigned(ui.menuSettled()),ui.stockRange(),unsigned(ui.stockRangePending()));
      else if(!strcmp(command_,"stock status")){
        for(unsigned i=0;i<stockService_.count();++i){auto s=stockService_.snapshot(i,4);Serial.printf("[BENCH] %s bars=%u %lu..%lu last=%.3f source=%s\n",s.ticker,unsigned(s.total),(unsigned long)s.firstDate,(unsigned long)s.lastDate,s.last,s.status);}
        Serial.printf("[BENCH] heap=%u PSRAM_free=%u uptime_ms=%lu\n",ESP.getFreeHeap(),ESP.getFreePsram(),(unsigned long)millis());
      }
      else if(!strncmp(command_,"range ",6)&&commandLength_==7&&command_[6]>='0'&&command_[6]<='4')ui.setStockRange(command_[6]-'0');
      else {Serial.println("[BENCH] commands: ui home/menu/stocks/spectrum/agent/next; range 0..4; stock status");commandLength_=0;continue;}
      Serial.printf("[BENCH] page=%u range=%u spectrum=%u picker=%u\n",unsigned(ui.page()),ui.stockRange(),ui.spectrumStyle(),unsigned(ui.spectrumPickerOpen()));commandLength_=0;
    }else if(commandLength_<sizeof(command_)-1)command_[commandLength_++]=c;
    else commandLength_=0;
  }
#endif
}

}  // namespace app
}  // namespace aurageek
