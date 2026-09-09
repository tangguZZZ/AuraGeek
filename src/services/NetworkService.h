#pragma once

#include <Arduino.h>

namespace aurageek {
namespace services {

class NetworkService {
 public:
  enum class State { kDisconnected, kConnecting, kConnected };

  struct Credential {
    const char* ssid;
    const char* password;
  };

  // Credentials are tried in the supplied order. Only this allow-list is used.
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
};

}  // namespace services
}  // namespace aurageek
