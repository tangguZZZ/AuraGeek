#include "drivers/Ws2812Driver.h"

#include <cmath>
#include <esp32-hal-rgb-led.h>
using aurageek::drivers::RgbColor;

namespace {

constexpr uint32_t kFramePeriodMs = 16;
constexpr uint32_t kBootDurationMs = 1800;
constexpr float kTwoPi = 6.28318530718F;

float clamp01(float value) {
  if (value < 0.0F) {
    return 0.0F;
  }
  if (value > 1.0F) {
    return 1.0F;
  }
  return value;
}

RgbColor makeColor(float hue, float saturation, float lightness) {
  const float l=clamp01(lightness),a=saturation*std::min(l,1.f-l);
  auto channel=[=](float n){float k=fmodf(n+hue*12.f,12.f);float v=l-a*std::max(-1.f,std::min(std::min(k-3.f,9.f-k),1.f));return uint8_t(powf(clamp01(v),2.2f)*255+.5f);};
  return RgbColor(channel(0),channel(8),channel(4));
}

}  // namespace

namespace aurageek {
namespace drivers {

Ws2812Driver::Ws2812Driver() = default;

void Ws2812Driver::begin() {
  show(RgbColor(0));

  state_ = State::kBoot;
  stateStartedMs_ = millis();
  lastFrameMs_ = 0;
  // Immediately show an obvious diagnostic colour. The non-blocking boot
  // sequence then cycles red, green, blue and white.
  show(RgbColor(0, 48, 64));
}

void Ws2812Driver::process() {
  const uint32_t now = millis();
  if (now - lastFrameMs_ < kFramePeriodMs) {
    return;
  }
  lastFrameMs_ = now;

  uint32_t elapsedMs = now - stateStartedMs_;
  if (state_ == State::kBoot && elapsedMs >= kBootDurationMs) {
    setState(State::kAmbient);
    elapsedMs = 0;
  }

  show(colorFor(state_, elapsedMs));
}

void Ws2812Driver::setState(State state) {
  if (state_ == state) {
    return;
  }
  state_ = state;
  stateStartedMs_ = millis();
}

Ws2812Driver::State Ws2812Driver::state() const {
  return state_;
}

float Ws2812Driver::normalizedPhase(uint32_t elapsedMs, uint32_t periodMs) {
  return static_cast<float>(elapsedMs % periodMs) /
         static_cast<float>(periodMs);
}

float Ws2812Driver::smoothPulse(float phase) {
  return 0.5F - 0.5F * cosf(kTwoPi * phase);
}

RgbColor Ws2812Driver::colorFor(State state, uint32_t elapsedMs) {
  switch (state) {
    case State::kBoot: {
      const uint32_t slot = elapsedMs / 450U;
      switch (slot) {
        case 0:
          return RgbColor(64, 0, 0);
        case 1:
          return RgbColor(0, 64, 0);
        case 2:
          return RgbColor(0, 0, 64);
        default:
          return RgbColor(48, 48, 48);
      }
    }

    case State::kAmbient: {
      const float huePhase = normalizedPhase(elapsedMs, 14000U);
      const float breath = smoothPulse(normalizedPhase(elapsedMs, 3600U));
      float hue = 0.48F + 0.40F * huePhase;
      if (hue >= 1.0F) {
        hue -= 1.0F;
      }
      return makeColor(hue, 0.82F, 0.045F + 0.13F * breath);
    }

    case State::kListening: {
      const float breath = smoothPulse(normalizedPhase(elapsedMs, 1400U));
      return makeColor(0.52F, 0.95F, 0.06F + 0.20F * breath);
    }

    case State::kThinking: {
      const float hue = 0.69F +
                        0.12F * normalizedPhase(elapsedMs, 1800U);
      const float breath = smoothPulse(normalizedPhase(elapsedMs, 900U));
      return makeColor(hue, 0.92F, 0.05F + 0.18F * breath);
    }

    case State::kSpeaking: {
      const float waveA = smoothPulse(normalizedPhase(elapsedMs, 620U));
      const float waveB = smoothPulse(normalizedPhase(elapsedMs + 170U, 970U));
      return makeColor(0.40F, 0.86F,
                       0.05F + 0.10F * waveA + 0.07F * waveB);
    }

    case State::kError: {
      const uint32_t slot = elapsedMs % 1600U;
      const bool lit = (slot < 130U) || (slot >= 260U && slot < 390U);
      return lit ? RgbColor(48, 0, 2) : RgbColor(0);
    }

    case State::kOff:
    default:
      return RgbColor(0);
  }
}

void Ws2812Driver::show(const RgbColor& color) {
  rgbLedWriteOrdered(board::kOnboardWs2812,LED_COLOR_ORDER_GRB,color.R,color.G,color.B);
}

}  // namespace drivers
}  // namespace aurageek
