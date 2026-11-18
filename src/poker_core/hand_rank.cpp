#include "poker_core/hand_rank.h"

#include <algorithm>

namespace poker::core {

const char* HandCategoryName(HandCategory category) noexcept {
    switch (category) {
    case HandCategory::HighCard:
        return "High Card";
    case HandCategory::OnePair:
        return "One Pair";
    case HandCategory::TwoPair:
        return "Two Pair";
    case HandCategory::ThreeOfAKind:
        return "Three of a Kind";
    case HandCategory::Straight:
        return "Straight";
    case HandCategory::Flush:
        return "Flush";
    case HandCategory::FullHouse:
        return "Full House";
    case HandCategory::FourOfAKind:
        return "Four of a Kind";
    case HandCategory::StraightFlush:
        return "Straight Flush";
    case HandCategory::RoyalFlush:
        return "Royal Flush";
    }
    return "Unknown";
}

int Compare(const HandRank& lhs, const HandRank& rhs) noexcept {
    const auto leftCategory = static_cast<std::uint8_t>(lhs.category);
    const auto rightCategory = static_cast<std::uint8_t>(rhs.category);
    if (leftCategory != rightCategory) {
        return leftCategory < rightCategory ? -1 : 1;
    }

    const std::uint8_t count =
        std::min(lhs.tiebreak_count, rhs.tiebreak_count);
    for (std::uint8_t i = 0; i < count; ++i) {
        if (lhs.tiebreak[i] != rhs.tiebreak[i]) {
            return lhs.tiebreak[i] < rhs.tiebreak[i] ? -1 : 1;
        }
    }

    if (lhs.tiebreak_count != rhs.tiebreak_count) {
        return lhs.tiebreak_count < rhs.tiebreak_count ? -1 : 1;
    }
    return 0;
}

bool operator==(const HandRank& lhs, const HandRank& rhs) noexcept {
    return Compare(lhs, rhs) == 0;
}

bool operator!=(const HandRank& lhs, const HandRank& rhs) noexcept {
    return !(lhs == rhs);
}

bool operator<(const HandRank& lhs, const HandRank& rhs) noexcept {
    return Compare(lhs, rhs) < 0;
}

bool operator<=(const HandRank& lhs, const HandRank& rhs) noexcept {
    return Compare(lhs, rhs) <= 0;
}

bool operator>(const HandRank& lhs, const HandRank& rhs) noexcept {
    return Compare(lhs, rhs) > 0;
}

bool operator>=(const HandRank& lhs, const HandRank& rhs) noexcept {
    return Compare(lhs, rhs) >= 0;
}

std::string ToString(const HandRank& rank) {
    std::string result = HandCategoryName(rank.category);
    if (rank.tiebreak_count == 0) {
        return result;
    }

    result += " (";
    for (std::uint8_t i = 0; i < rank.tiebreak_count; ++i) {
        if (i != 0) {
            result += ", ";
        }
        result += std::to_string(rank.tiebreak[i]);
    }
    result += ')';
    return result;
}

} // namespace poker::core
