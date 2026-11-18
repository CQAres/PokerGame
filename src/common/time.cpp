#include "common/time.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace poker::common {
namespace {

std::tm ToUtcTm(std::time_t time) {
    std::tm result{};
#ifdef _WIN32
    gmtime_s(&result, &time);
#else
    gmtime_r(&time, &result);
#endif
    return result;
}

} // namespace

int64_t UnixMillis(SystemClock::time_point time) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               time.time_since_epoch())
        .count();
}

std::string FormatIso8601Utc(SystemClock::time_point time) {
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        time.time_since_epoch());
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(milliseconds);
    auto remainder = milliseconds - seconds;
    if (remainder.count() < 0) {
        remainder += std::chrono::seconds(1);
        seconds -= std::chrono::seconds(1);
    }

    const std::time_t rawTime = static_cast<std::time_t>(seconds.count());
    const std::tm utc = ToUtcTm(rawTime);

    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.'
           << std::setw(3) << std::setfill('0') << remainder.count() << 'Z';
    return output.str();
}

std::string NowIso8601Utc() {
    return FormatIso8601Utc(SystemClock::now());
}

} // namespace poker::common
