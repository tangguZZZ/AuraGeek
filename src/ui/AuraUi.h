#pragma once
#include <lvgl.h>
#include <cstddef>
#include <cstdint>
#include "services/StockChart.h"

namespace aurageek { namespace ui {
class AuraUi {
 public:
  enum class Page { Home, Menu, Stocks, Spectrum, Agent };
  bool create();
  void rotate(int steps);
  void click();
  void back();
  void home();
  void show(Page page);
  Page page() const { return page_; }
  int selection() const { return selection_; }
  unsigned stockIndex() const { return stockIndex_; }
  unsigned stockRange() const {return stockRange_;}
  void setStockRange(unsigned range){stockRange_=range<5?range:4;stock_.count=0;}
  void setEpoch(uint32_t epoch) {if(second_!=int(epoch%60)){second_=epoch%60;lastClockTick_=lv_tick_get();}if(epoch>86400)dayIndex_=int((epoch/86400+3)%7);}
  void setNetwork(bool connected, bool connecting, int rssi);
  void setClock(bool valid, const char* time, const char* date);
  void setWeather(bool valid, const char* text, int code);
  void setIndoorUnavailable();
  void setAiPending();
  void setStock(const services::StockChart& chart);
  void setSpectrum(const float* bands, size_t count, bool active, bool demo = false);
 private:
  static void timer(lv_timer_t* timer);
  static void draw(lv_event_t* e);
  static void pressed(lv_event_t* e);
  static void leaveCompleted(lv_anim_t* anim);
  void load(Page page);
  void animateMenuSelection(int previous, int current);
  void buildHome();
  void buildMenu();
  void buildStocks();
  void buildSpectrum();
  void buildAgent();
  void renderData();
  lv_obj_t *root_=nullptr, *clock_=nullptr, *date_=nullptr, *weather_=nullptr;
  lv_obj_t *weatherIcon_=nullptr, *visual_=nullptr, *secondsVisual_=nullptr;
  lv_obj_t *weatherFlow_=nullptr, *dateFlow_=nullptr, *status_=nullptr;
  lv_obj_t *change_=nullptr,*rangeLabels_[5]{},*rangeButtons_[5]{},*dateStart_=nullptr,*dateEnd_=nullptr;
  lv_obj_t *wifiIcon_=nullptr,*scaleLabels_[3]{},*dateDial_=nullptr;
  lv_obj_t *menuCards_[4]{};
  lv_timer_t* timer_=nullptr;
  Page page_=Page::Home;
  Page pendingPage_=Page::Home;
  bool transitioning_=false;
  int selection_=0;
  unsigned stockIndex_=0;
  unsigned stockRange_=4;
  uint32_t lastClockTick_=0;
  int second_=0;
  int dayIndex_=0;
  int rssi_=-100;
  bool clockValid_=false, weatherValid_=false, connected_=false, audioActive_=false, demoAudio_=false;
  char time_[16]="--:--", dateText_[48]="WAITING FOR NETWORK", weatherText_[48]="未知 --℃";
  int weatherCode_=-1;
  float bands_[48]{},peaks_[48]{};
  services::StockChart stock_;
};
}}
