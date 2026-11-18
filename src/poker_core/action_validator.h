#pragma once

#include <cstdint>

#include "common/result.h"
#include "poker_core/action.h"
#include "poker_core/game_state.h"

namespace poker::core {

std::int64_t ToCall(const GameState& state, const Player& player) noexcept;
std::int64_t MaxTarget(const Player& player) noexcept;

// Minimum legal target_total for the next bet/raise in this round.
std::int64_t ComputeMinRaiseTo(const GameState& state) noexcept;

int CountActionableOpponents(const GameState& state, int seat) noexcept;

// A player may only put more chips in the pot when at least one opponent can
// still respond and the player has not acted since the last full raise.
bool CanPlayerRaise(const GameState& state, const Player& player) noexcept;

// Builds the finite, explainable candidate set described in the design doc.
// Turn order is checked by the caller (GameEngine / ActionValidator).
LegalActions BuildLegalActions(const GameState& state, const Player& player);

class ActionValidator {
public:
    // Validates a concrete action, including custom human bet/raise sizes.
    // Returns an error Status for every illegal action.
    static common::Status Validate(const GameState& state,
                                   const Player& player,
                                   const LegalActions& legal,
                                   const Action& action);
};

} // namespace poker::core
