#pragma once

#include <Arduino.h>

#include "board/BoardPins.h"

namespace aurageek {
namespace drivers {
struct RgbColor {
  uint8_t R,G,B;
  explicit RgbColor(uint8_t v=0):R(v),G(v),B(v){}
  RgbColor(uint8_t r,uint8_t g,uint8_t b):R(r),G(g),B(b){}
};

class Ws2812Driver {
 public:
  enum class State : uint8_t {
    kBoot,
    kAmbient,
    kListening,
    kThinking,
    kSpeaking,
    kError,
    kOff,
  };

  Ws2812Driver();

  void begin();
  void process();
  void setState(State state);
  State state() const;

 private:

  static float normalizedPhase(uint32_t elapsedMs, uint32_t periodMs);
  static float smoothPulse(float phase);
  static RgbColor colorFor(State state, uint32_t elapsedMs);

  void show(const RgbColor& color);

  State state_ = State::kBoot;
  uint32_t stateStartedMs_ = 0;
  uint32_t lastFrameMs_ = 0;
};

}  // namespace drivers
}  // namespace aurageek
