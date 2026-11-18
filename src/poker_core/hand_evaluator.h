#pragma once

#include <array>
#include <vector>

#include "common/result.h"
#include "poker_core/card.h"
#include "poker_core/hand_rank.h"

namespace poker::core {

// Evaluates the best five-card hand from 5, 6, or 7 cards.
// Returns invalid_hand_size, invalid_card, or duplicate_card on bad input.
common::Result<HandRank> EvaluateHand(const std::vector<Card>& cards);

// Evaluates exactly five cards and rejects duplicates.
common::Result<HandRank> EvaluateFive(const std::array<Card, 5>& cards);

} // namespace poker::core
