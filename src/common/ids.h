#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace poker::common {

// Process-local monotonically increasing identifier.
uint64_t NextId() noexcept;

// Timestamp + process-local counter, suitable for logs and request IDs.
std::string MakeId(std::string_view prefix = {});

} // namespace poker::common
