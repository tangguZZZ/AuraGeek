#pragma once
#include <Arduino.h>
#include <atomic>
#include "services/StockChart.h"
#include "services/StockPreferences.h"
namespace aurageek { namespace services {
class StockService {
 public:
  void begin();
  void configure(const StockPreferences& preferences){if(!worker_){std::string error;if(preferences.valid(error))preferences_=preferences;}}
  void process(bool connected,uint32_t utcEpoch);
  StockChart snapshot(unsigned index,unsigned range) const;
  unsigned count() const {return preferences_.count;}
 private:
  static void task(void* p);
  bool fetch(unsigned index,uint32_t day,int& code,uint32_t& retryAfter);
  void load(unsigned index);
  bool save(unsigned index,const StockBar* bars,size_t count);
  void setStatus(unsigned index,const char* status);
  TaskHandle_t worker_=nullptr;
  mutable SemaphoreHandle_t lock_=nullptr;
  StockPreferences preferences_;
  StockBar* bars_[StockPreferences::capacity]{};
  size_t counts_[StockPreferences::capacity]{};
  char status_[StockPreferences::capacity][64]{};
  bool storageReady_=false;
  uint32_t attemptDay_=0;
  std::atomic<uint32_t> pendingDay_{0};
  uint32_t lastWakeMs_=0;
};
}}
