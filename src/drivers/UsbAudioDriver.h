#pragma once
#include <Arduino.h>
#include <atomic>
#include "services/SpectrumAnalyzer.h"
namespace aurageek { namespace drivers {
// USB receive callback only fills a bounded PCM ring; FFT/UI run in loop().
class UsbAudioDriver {
 public:
  bool begin();
  void process();
  bool active() const;
  const float* bands() const {return fft_.bands();}
 private:
  static void receive(void* data,uint16_t bytes);
  static UsbAudioDriver* instance_;
  portMUX_TYPE lock_=portMUX_INITIALIZER_UNLOCKED;
  int16_t ring_[2048]{};
  unsigned write_=0,read_=0;
  std::atomic<uint32_t> lastPcm_{0};
  uint32_t packets_=0,overruns_=0,lastFft_=0,lastLog_=0;
  services::SpectrumAnalyzer fft_;
};
}}
