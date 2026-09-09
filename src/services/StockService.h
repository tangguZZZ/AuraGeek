#pragma once
#include <Arduino.h>
#include <atomic>
#include "services/StockChart.h"
namespace aurageek { namespace services {
class StockService {
 public:
  void begin();
  void process(bool connected,uint32_t localEpoch);
  StockChart snapshot(unsigned index,unsigned range) const;
 private:
  static void task(void* p);
  bool fetch(unsigned index,uint32_t day,int& code,uint32_t& retryAfter);
  void load(unsigned index);
  bool save(unsigned index,const StockBar* bars,size_t count);
  void setStatus(unsigned index,const char* status);
  TaskHandle_t worker_=nullptr;
  mutable SemaphoreHandle_t lock_=nullptr;
  StockBar* bars_[2]{};
  size_t counts_[2]{};
  char status_[2][64]{"REF CACHE / OFFLINE","REF CACHE / OFFLINE"};
  bool storageReady_=false;
  uint32_t attemptDay_=0;
  std::atomic<uint32_t> pendingDay_{0};
  uint32_t lastWakeMs_=0;
};
}}
