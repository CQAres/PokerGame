#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace poker::core {

// Ordered from weakest to strongest. RoyalFlush is kept as a distinct
// category so the CLI can display it directly.
enum class HandCategory : std::uint8_t {
    HighCard = 0,
    OnePair,
    TwoPair,
    ThreeOfAKind,
    Straight,
    Flush,
    FullHouse,
    FourOfAKind,
    StraightFlush,
    RoyalFlush,
};

const char* HandCategoryName(HandCategory category) noexcept;

// A comparable hand value. tiebreak is ordered from most significant to
// least significant, e.g. two pair = {high_pair, low_pair, kicker}.
struct HandRank {
    HandCategory category = HandCategory::HighCard;
    std::array<std::uint8_t, 5> tiebreak{};
    std::uint8_t tiebreak_count = 0;
};

// Returns <0, 0, >0 when lhs is weaker than, equal to, or stronger than rhs.
int Compare(const HandRank& lhs, const HandRank& rhs) noexcept;

bool operator==(const HandRank& lhs, const HandRank& rhs) noexcept;
bool operator!=(const HandRank& lhs, const HandRank& rhs) noexcept;
bool operator<(const HandRank& lhs, const HandRank& rhs) noexcept;
bool operator<=(const HandRank& lhs, const HandRank& rhs) noexcept;
bool operator>(const HandRank& lhs, const HandRank& rhs) noexcept;
bool operator>=(const HandRank& lhs, const HandRank& rhs) noexcept;

std::string ToString(const HandRank& rank);

} // namespace poker::core
