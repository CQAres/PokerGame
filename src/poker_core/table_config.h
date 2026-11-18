#pragma once

#include <cstdint>
#include <string>

#include "common/result.h"

namespace poker::core {

struct TableConfig {
    std::string table_id = "t1";
    int seat_count = 6;
    std::int64_t starting_stack = 1000;
    std::int64_t small_blind = 5;
    std::int64_t big_blind = 10;
    int first_button_seat = 0;
    std::uint64_t seed = 0; // 0 = derive a random seed per engine instance
    int hands_per_session = 20;
};

common::Status ValidateTableConfig(const TableConfig& config);

} // namespace poker::core
