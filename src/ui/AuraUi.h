#pragma once
#include <lvgl.h>
#include <cstddef>
#include <cstdint>
#include "services/StockChart.h"
#include "services/StockPreferences.h"
#include "ui/SpectrumView.h"
#include "ui/SettingsView.h"

namespace aurageek { namespace ui {
class AuraUi {
 public:
  enum class Page { Home, Menu, Stocks, Spectrum, Agent, Settings };
  UserSettings settings;
  bool takeSettingsCommit(){return settingsView_.takeCommit();}
  bool takePortalRequest(){return settingsView_.takePortalRequest();}
  void applySettings(){auto style=settings.spectrum?SpectrumView::Style::Ring:SpectrumView::Style::Reflection;if(spectrumView_.style()!=style)spectrumView_.setStyle(style);}
  void settingsSaveResult(bool ok){settingsView_.saveResult(ok);}
  bool create();
  void configureHome(uint32_t background,uint32_t foreground,uint32_t accent,const char* city);
  void configureStocks(const services::StockPreferences& preferences);
  void rotate(int steps);
  void click();
  void back();
  void doubleClick();
  bool spectrumPickerOpen() const {return spectrumPicker_!=nullptr;}
  unsigned spectrumStyle() const {return unsigned(spectrumView_.style());}
  uint32_t spectrumRenderMs() const {return spectrumView_.renderMs();}
  void home();
  void show(Page page);
  Page page() const { return page_; }
  int selection() const { return selection_; }
  bool menuSettled() const {return menuPosition_==menuTarget_&&menuVelocity_==0.f;}
  unsigned stockIndex() const { return stockIndex_; }
  unsigned stockRange() const {return stockRange_;}
  void setStockRange(unsigned range){stockRange_=range<5?range:4;}
  bool stockRangePending() const {return stockRange_!=stockDisplayedRange_;}
  void setEpoch(uint32_t epoch) {if(second_!=int(epoch%60)){second_=epoch%60;lastClockTick_=lv_tick_get();}if(epoch>86400)dayIndex_=int((epoch/86400+3)%7);}
  void setNetwork(bool connected, bool connecting, int rssi);
  void setClock(bool valid, const char* time, const char* date);
  void setWeather(bool valid, const char* text, int code,
                  const float* hourlyTemperature = nullptr,
                  size_t hourlyCount = 0);
  void setIndoorUnavailable();
  void setIndoor(bool valid, float temperature, float humidity);
  void setAiPending();
  void setAgentState(unsigned state);
  void setAgentEmotion(unsigned emotion);
  unsigned agentFaceRevision() const {return agentFaceRevision_;}
  bool agentFaceLoaded() const {return agentGif_ && lv_gif_is_loaded(agentGif_);}
  void setStock(const services::StockChart& chart);
  void setSpectrum(const float* bands, size_t count, bool active, bool demo = false);
 private:
  uint32_t homeBackground_=0x050505,homeForeground_=0xf2f2f2,homeAccent_=0xdfff00;
  uint32_t homeDim_=0x8a8a8a,homeFaint_=0x4b4b4b,homeLine_=0x1e1e1e;
  char homeCity_[21]="SHENZHEN";
  static void timer(lv_timer_t* timer);
  static void draw(lv_event_t* e);
  static void pressed(lv_event_t* e);
  static void transitionStep(void* context,int32_t value);
  static void transitionCompleted(lv_anim_t* anim);
  static void pickerDraw(lv_event_t* event);
  void openSpectrumPicker();
  void closeSpectrumPicker(bool confirm);
  void renderSpectrumPicker();
  void load(Page page);
  void animateMenuSelection(int previous, int current);
  void renderMenuCarousel();
  void buildHome();
  void buildMenu();
  void buildStocks();
  void buildSpectrum();
  void buildAgent();
  void renderData();
  lv_obj_t *root_=nullptr, *clock_=nullptr, *date_=nullptr, *weather_=nullptr;
  lv_obj_t* agentStatus_=nullptr;
  lv_obj_t* agentGif_=nullptr;
  const void* agentGifSource_=nullptr;
  const void* agentFacePending_=nullptr;
  unsigned agentFacePhase_=0;
  static void agentFaceOpacity(void* self,int32_t value);
  static void agentFaceFadeOutDone(lv_anim_t* anim);
  static void agentFaceFadeInDone(lv_anim_t* anim);
  unsigned agentEmotion_=0;
  unsigned agentFaceRevision_=0;
  void renderAgentFace();
  unsigned agentState_=0;
  lv_obj_t *weatherIcon_=nullptr, *visual_=nullptr, *secondsVisual_=nullptr;
  lv_obj_t *weatherFlow_=nullptr, *dateFlow_=nullptr, *status_=nullptr;
  lv_obj_t *change_=nullptr,*rangeLabels_[5]{},*rangeButtons_[5]{},*dateStart_=nullptr,*dateEnd_=nullptr;
  lv_obj_t *wifiIcon_=nullptr,*scaleLabels_[3]{},*dateDial_=nullptr;
  lv_obj_t *homeDateDay_=nullptr,*homeDateRest_=nullptr,*homeNetworkMeta_=nullptr;
  lv_obj_t *clockMinute_=nullptr,*clockColon_=nullptr;
  lv_obj_t *indoorTemp_=nullptr,*indoorHumidity_=nullptr;
  lv_obj_t *menuCards_[5]{},*menuGlyphs_[5]{},*menuLabels_[5]{},*menuDots_[5]{},*menuTitle_=nullptr,*menuCounter_=nullptr,*menuClock_=nullptr;
  SettingsView settingsView_;
  lv_timer_t* timer_=nullptr;
  Page page_=Page::Home;
  Page pendingPage_=Page::Home;
  bool transitioning_=false;
  lv_draw_buf_t* transitionSnapshot_=nullptr;
  lv_obj_t *transitionCover_=nullptr,*transitionImage_=nullptr;
  lv_obj_t *spectrumPicker_=nullptr,*styleCards_[unsigned(SpectrumView::Style::Count)]{};
  int styleSelection_=0;
  float stylePosition_=0.f,styleVelocity_=0.f;
  bool deferredDouble_=false;
  Page deferredDoublePage_=Page::Home;
  uint32_t frameTick_=0;
  SpectrumView spectrumView_;
  int selection_=0;
  float menuPosition_=0.f,menuTarget_=0.f,menuVelocity_=0.f;
  float menuScale_[5]{1,1,1,1,1};
  float menuFractionX_[5]{},menuFractionY_[5]{};
  uint8_t menuOpacity_[5]{255,255,255,255,255};
  uint32_t menuTick_=0;
  unsigned stockIndex_=0;
  services::StockPreferences stockPreferences_;
  lv_obj_t *stockCurrency_=nullptr,*stockFast_=nullptr,*stockSlow_=nullptr;
  lv_obj_t *stockReturnScope_=nullptr,*stockPosition_=nullptr,*stockEmpty_=nullptr,*stockEmptyHelp_=nullptr;
  unsigned stockRange_=4;
  unsigned stockDisplayedRange_=4;
  uint32_t lastClockTick_=0;
  int second_=0;
  int dayIndex_=0;
  int rssi_=-100;
  bool clockValid_=false, weatherValid_=false, connected_=false, audioActive_=false, demoAudio_=false;
  bool indoorValid_=false;
  float indoorTemperature_=0.f,indoorHumidityValue_=0.f;
  char time_[16]="--:--", dateText_[48]="WAITING FOR NETWORK", weatherText_[48]="未知 --℃";
  int weatherCode_=-1;
  float weatherTrend_[24]{};
  size_t weatherTrendCount_=0;
  float weatherTrendMin_=0.f,weatherTrendMax_=0.f;
  float bands_[48]{},peaks_[48]{};
  services::StockChart stock_;
};
}}
