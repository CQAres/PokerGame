#include "common/ids.h"

#include <atomic>
#include <iomanip>
#include <sstream>

#include "common/time.h"

namespace poker::common {
namespace {

std::atomic<uint64_t> g_nextId{1};

} // namespace

uint64_t NextId() noexcept {
    return g_nextId.fetch_add(1, std::memory_order_relaxed);
}

std::string MakeId(std::string_view prefix) {
    const uint64_t sequence = NextId();
    const uint64_t timestamp =
        static_cast<uint64_t>(UnixMillis(SystemClock::now()));

    std::ostringstream output;
    output << prefix << std::hex << std::setfill('0') << std::setw(12)
           << timestamp << std::setw(8) << (sequence & 0xFFFFFFFFu);
    return output.str();
}

} // namespace poker::common
