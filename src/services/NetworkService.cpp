#include "services/NetworkService.h"

#include <WiFi.h>

namespace {
constexpr uint32_t kConnectTimeoutMs = 15000U;
constexpr uint32_t kFullCycleRetryIntervalMs = 10000U;
}

namespace aurageek {
namespace services {

void NetworkService::begin(const Credential* credentials, size_t credentialCount) {
  credentials_ = credentials;
  credentialCount_ = credentialCount;
  credentialIndex_ = 0;
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setSleep(false);
  startConnection();
}

void NetworkService::process() {
  const uint32_t now = millis();
  const wl_status_t wifiStatus = WiFi.status();

  if (wifiStatus == WL_CONNECTED) {
    if (state_ != State::kConnected) {
      state_ = State::kConnected;
      stateStartedMs_ = now;
      Serial.printf("[NET] Connected to %s, IP=%s, RSSI=%d dBm\n",
                    WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
                    WiFi.RSSI());
    }
    return;
  }

  if (state_ == State::kConnected) {
    state_ = State::kDisconnected;
    stateStartedMs_ = now;
    Serial.println("[NET] Wi-Fi disconnected");
  }

  if (state_ == State::kConnecting && now - stateStartedMs_ >= kConnectTimeoutMs) {
    WiFi.disconnect(false, false);
    state_ = State::kDisconnected;
    stateStartedMs_ = now;
    Serial.printf("[NET] Connect timeout: %s\n",
                  credentials_[credentialIndex_].ssid);
    scheduleNextNetwork();
  }

  if (state_ == State::kDisconnected &&
      static_cast<int32_t>(now - nextAttemptMs_) >= 0) {
    startConnection();
  }
}

bool NetworkService::connected() const { return state_ == State::kConnected; }
bool NetworkService::connecting() const { return state_ == State::kConnecting; }
int NetworkService::rssi() const { return connected() ? WiFi.RSSI() : -127; }
NetworkService::State NetworkService::state() const { return state_; }

void NetworkService::startConnection() {
  if (credentials_ == nullptr || credentialCount_ == 0 ||
      credentials_[credentialIndex_].ssid == nullptr ||
      credentials_[credentialIndex_].ssid[0] == '\0') {
    Serial.println("[NET] Wi-Fi credential allow-list is empty or invalid");
    return;
  }
  stateStartedMs_ = millis();
  nextAttemptMs_ = stateStartedMs_;
  state_ = State::kConnecting;
  const Credential& credential = credentials_[credentialIndex_];
  Serial.printf("[NET] Connecting to %s (%u/%u)\n", credential.ssid,
                static_cast<unsigned>(credentialIndex_ + 1U),
                static_cast<unsigned>(credentialCount_));
  WiFi.begin(credential.ssid, credential.password);
}

void NetworkService::scheduleNextNetwork() {
  if (credentialCount_ == 0) {
    return;
  }

  credentialIndex_ = (credentialIndex_ + 1U) % credentialCount_;
  if (credentialIndex_ == 0) {
    nextAttemptMs_ = millis() + kFullCycleRetryIntervalMs;
    Serial.println("[NET] All allowed Wi-Fi networks failed; next cycle scheduled");
  } else {
    nextAttemptMs_ = millis();
  }
}

}  // namespace services
}  // namespace aurageek
