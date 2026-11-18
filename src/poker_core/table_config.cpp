#include "poker_core/table_config.h"

namespace poker::core {

common::Status ValidateTableConfig(const TableConfig& config) {
    if (config.seat_count < 2 || config.seat_count > 6) {
        return common::MakeError(
            "invalid_seat_count",
            "seat_count must be between 2 and 6, got " +
                std::to_string(config.seat_count));
    }
    if (config.starting_stack <= 0) {
        return common::MakeError(
            "invalid_starting_stack",
            "starting_stack must be positive, got " +
                std::to_string(config.starting_stack));
    }
    if (config.small_blind <= 0 || config.big_blind <= 0) {
        return common::MakeError(
            "invalid_blinds", "small_blind and big_blind must be positive");
    }
    if (config.small_blind > config.big_blind) {
        return common::MakeError(
            "invalid_blinds", "small_blind must not exceed big_blind");
    }
    if (config.first_button_seat < 0 ||
        config.first_button_seat >= config.seat_count) {
        return common::MakeError(
            "invalid_first_button_seat",
            "first_button_seat is outside the table");
    }
    if (config.hands_per_session <= 0) {
        return common::MakeError(
            "invalid_hands_per_session",
            "hands_per_session must be positive");
    }
    return common::Status::Ok();
}

} // namespace poker::core
