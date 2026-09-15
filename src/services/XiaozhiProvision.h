#pragma once
#include <Arduino.h>
#include <atomic>

namespace aurageek::services {
// Provisioning only: never downloads or installs a server-offered firmware.
class XiaozhiProvision {
 public:
  bool request();
  void printStatus() const;
 private:
  static void taskEntry(void* self);
  void check();
  std::atomic<bool> busy_{false};
  std::atomic<int> status_{0}; // 0 unchecked, 1 activation, 2 configured, -1 error
};
}
