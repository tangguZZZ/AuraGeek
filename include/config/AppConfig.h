#pragma once

#include <stdint.h>

namespace aurageek {
namespace config {

constexpr int32_t kUtcOffsetSeconds = 8 * 60 * 60;
constexpr uint32_t kNtpUpdateIntervalMs = 60U * 1000U;
constexpr char kNtpServer[] = "ntp.aliyun.com";

// MVP weather location: Shenzhen. Keep location policy separate from the UI.
constexpr double kWeatherLatitude = 22.5431;
constexpr double kWeatherLongitude = 114.0579;
constexpr uint32_t kWeatherUpdateIntervalMs = 15U * 60U * 1000U;

}  // namespace config
}  // namespace aurageek
