#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "poker_core/card.h"

namespace poker::core {

enum class PlayerType : std::uint8_t {
    Human = 0,
    Rule,
    Llm,
    Random,
};

const char* PlayerTypeName(PlayerType type) noexcept;

inline constexpr std::uint64_t kEmptyPlayerId = 0;

struct Player {
    std::uint64_t id = kEmptyPlayerId;
    int seat = -1;
    std::string name;
    PlayerType type = PlayerType::Human;

    std::int64_t stack = 0;
    std::int64_t committed_hand = 0;
    std::int64_t committed_round = 0;

    bool folded = false;
    bool all_in = false;
    bool sitting_out = false;
    bool disconnected = false;
    bool is_active = false; // dealt into the current hand
    bool needs_action = false;
    bool acted_since_full_raise = false;
    bool cards_visible = false;

    std::vector<Card> hole_cards;
    std::string last_action;

    [[nodiscard]] bool IsSeated() const noexcept {
        return id != kEmptyPlayerId && seat >= 0;
    }

    [[nodiscard]] bool IsActionable() const noexcept {
        return IsSeated() && is_active && !folded && !all_in &&
               !sitting_out && stack > 0;
    }
};

} // namespace poker::core
