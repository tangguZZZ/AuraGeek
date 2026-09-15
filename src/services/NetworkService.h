#pragma once

#include <Arduino.h>
#include "services/NetworkCredentials.h"

namespace aurageek {
namespace services {

class NetworkService {
 public:
  enum class State { kDisconnected, kConnecting, kConnected };

  struct Credential {
    const char* ssid;
    const char* password;
  };

  // Prefer a saved provisioned network; otherwise use the development allow-list.
  void begin(const Credential* credentials, size_t credentialCount);
  void process();
  bool connected() const;
  bool connecting() const;
  int rssi() const;
  State state() const;

 private:
  void startConnection();
  void scheduleNextNetwork();

  const Credential* credentials_ = nullptr;
  size_t credentialCount_ = 0;
  size_t credentialIndex_ = 0;
  State state_ = State::kDisconnected;
  uint32_t stateStartedMs_ = 0;
  uint32_t nextAttemptMs_ = 0;
  NetworkCredentials savedNetwork_;
  Credential savedCredential_{};
};

}  // namespace services
}  // namespace aurageek
