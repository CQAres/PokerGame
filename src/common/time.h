#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace poker::common {

using SystemClock = std::chrono::system_clock;

int64_t UnixMillis(SystemClock::time_point time = SystemClock::now());

std::string FormatIso8601Utc(SystemClock::time_point time);

std::string NowIso8601Utc();

} // namespace poker::common
