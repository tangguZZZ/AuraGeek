#pragma once

#include <Arduino.h>
#include <atomic>
#include <driver/i2s_std.h>
#include <freertos/queue.h>
#include "drivers/WakeWord.h"
#include "services/PortalConfig.h"

namespace aurageek::drivers {

// One worker owns both I2S channels. No audio waits run in the GUI task.
class VoiceAudio {
 public:
  enum class Command : uint8_t { Tone, Record, Play, Chat, WakeChat };
  enum class ChatState : uint8_t { Idle, Connecting, Listening, Thinking, Speaking, Error };
  bool toggleChat(bool fromWake=false);
  void cancelChat() { if(chatState_!=ChatState::Idle && chatState_!=ChatState::Error) cancelled_=true; }
  ChatState chatState() const { return chatState_.load(); }
  unsigned emotion() const { return emotion_.load(); }
  bool takeWakeRequest() { return wakePending_.exchange(false); }
  void setWakeEnabled(bool enabled) { wakeEnabled_=enabled; }
  // Boot-only: configure before starting the voice worker, never mutate mid-session.
  void configure(const services::PortalConfig& config){config_=config;wakeEnabled_=config.wakeEnabled;}
  void retryWake() { wakeRetry_=true; }
  bool begin();
  bool request(Command command);
  void stop() { cancelled_.store(true); usbEnabled_.store(false); }
  void enableUsb() { usbEnabled_.store(true); }
  void setVolume(unsigned percent){volumePercent_=percent>100?100:percent;}
  unsigned volume() const{return volumePercent_.load();}
  void acceptUsb(const int16_t* stereo, size_t frames);
  void printStatus() const;

 private:
  services::PortalConfig config_;
  static void taskEntry(void* self);
  void run();
  bool initialize();
  bool initializeMic();
  void cleanup();
  bool output(const int16_t* mono, size_t count);
  void silence();
  bool record();
  bool play(bool tone);
  bool chat(bool greetOnWake=false);
  bool setOutputRate(unsigned rate);
  void processUsb();
  void clearUsb();
  void processWake();
  void releaseMic();
  WakeWord wake_;
  bool wakeAttempted_=false;
  uint32_t wakeResumeAt_=0;
  float wakeDc_=0;
  std::atomic<bool> wakeEnabled_{true},wakePending_{false};
  std::atomic<bool> wakeReady_{false},wakeRetry_{false};
  std::atomic<unsigned> emotion_{0};
  QueueHandle_t commands_ = nullptr;
  i2s_chan_handle_t rx_ = nullptr, tx_ = nullptr;
  int16_t* recording_ = nullptr;
  size_t recorded_ = 0;
  bool rxEnabled_ = false, txEnabled_ = false;
  std::atomic<bool> ready_{false}, busy_{false}, cancelled_{false};
  std::atomic<uint32_t> samples_{0}, peak_{0}, rms_{0}, clipped_{0}, errors_{0};
  std::atomic<bool> usbEnabled_{true};
  std::atomic<uint32_t> usbDropped_{0}, usbPlayed_{0}, usbUnderruns_{0};
  portMUX_TYPE usbLock_ = portMUX_INITIALIZER_UNLOCKED;
  int16_t* usbRing_=nullptr;
  unsigned usbRead_=0, usbWrite_=0;
  uint32_t lastUsbMs_=0;
  bool usbPlaying_=false;
  bool usbRefilling_=false;
  std::atomic<unsigned> volumePercent_{100};
  int gainQ15_=0;
  std::atomic<ChatState> chatState_{ChatState::Idle};
  std::atomic<bool> finishListening_{false};
};
}
