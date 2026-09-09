#include "services/TimeService.h"

#include <Arduino.h>
#include <time.h>
#include <sys/time.h>

#include "config/AppConfig.h"

namespace {
constexpr unsigned long kEarliestValidEpoch = 1704067200UL;  // 2024-01-01
const char* const kWeekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* const kMonths[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                               "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
}

namespace aurageek {
namespace services {

TimeService::TimeService()
    : client_(udp_, config::kNtpServer, config::kUtcOffsetSeconds,
              config::kNtpUpdateIntervalMs) {}

void TimeService::begin() {
  Serial.println("[TIME] NTP deferred until Wi-Fi is connected");
}

void TimeService::process(bool networkConnected) {
  if (!networkConnected) {
    return;
  }
  if (!clientStarted_) {
    client_.begin();
    clientStarted_ = true;
    Serial.println("[TIME] Wi-Fi ready; NTP client started");
  }
  if (client_.update() && client_.getEpochTime() >= kEarliestValidEpoch) {
    if (!synchronized_) {
      Serial.println("[TIME] NTP synchronized");
    }
    synchronized_ = true;
    // NTPClient applies the display timezone. POSIX time and persisted retry
    // deadlines must be UTC, not the shifted display epoch.
    timeval utc{static_cast<time_t>(client_.getEpochTime()-config::kUtcOffsetSeconds),0};
    // update() also returns true between NTP exchanges; retain subsecond time.
    const double drift=difftime(utc.tv_sec,time(nullptr));
    if(drift>2.0||drift< -2.0)settimeofday(&utc,nullptr);
  }
}

bool TimeService::valid(bool networkConnected) const {
  return networkConnected && synchronized_ &&
         client_.getEpochTime() >= kEarliestValidEpoch;
}

void TimeService::formatTime(char* output, size_t outputSize) const {
  snprintf(output, outputSize, "%02d:%02d", client_.getHours(), client_.getMinutes());
}

void TimeService::formatDate(char* output, size_t outputSize) const {
  const time_t epoch = static_cast<time_t>(client_.getEpochTime());
  struct tm localTime {};
  gmtime_r(&epoch, &localTime);
  snprintf(output, outputSize, "%s  %s %02d  %04d", kWeekdays[localTime.tm_wday],
           kMonths[localTime.tm_mon], localTime.tm_mday, localTime.tm_year + 1900);
}

}  // namespace services
}  // namespace aurageek
