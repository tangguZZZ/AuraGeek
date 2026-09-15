#pragma once
#include <cstdint>

namespace aurageek::services {
// The gesture used to enter setup must never also leave setup. Drain events
// until both buttons have been released for longer than the gesture window.
class PortalExitGuard {
 public:
  bool accepts(bool buttonsReleased, uint32_t now, uint32_t eventAt) {
    if (!armed_) {
      if (!buttonsReleased) released_ = false;
      else if (!released_) { released_ = true; releasedAt_ = now; }
      else if (uint32_t(now - releasedAt_) >= 600) {
        armed_ = true;
        armedAt_ = now;
      }
      return false;
    }
    return int32_t(eventAt - armedAt_) > 0;
  }
 private:
  bool released_ = false, armed_ = false;
  uint32_t releasedAt_ = 0, armedAt_ = 0;
};
}
