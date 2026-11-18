#include "poker_core/hand_evaluator.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <string>

namespace poker::core {
namespace {

common::Status ValidateCard(const Card& card, bool seen[52]) {
    if (!card.IsValid()) {
        return common::MakeError("invalid_card",
                                 "invalid card rank/suit: " +
                                     std::to_string(card.rank));
    }

    const int index = CardIndex(card);
    if (seen[index]) {
        return common::MakeError("duplicate_card",
                                 "duplicate card: " + ToString(card));
    }
    seen[index] = true;
    return common::Status::Ok();
}

HandRank MakeRank(HandCategory category,
                  std::initializer_list<int> tiebreak) {
    HandRank rank;
    rank.category = category;
    for (const int value : tiebreak) {
        if (rank.tiebreak_count >= rank.tiebreak.size()) {
            break;
        }
        rank.tiebreak[rank.tiebreak_count++] =
            static_cast<std::uint8_t>(value);
    }
    return rank;
}

HandRank EvaluateFiveUnchecked(const std::array<Card, 5>& cards) {
    std::array<int, 15> rankCounts{};
    std::array<int, 4> suitCounts{};
    std::array<std::uint8_t, 5> sortedRanks{};

    for (std::size_t i = 0; i < cards.size(); ++i) {
        const Card& card = cards[i];
        ++rankCounts[card.rank];
        ++suitCounts[static_cast<std::size_t>(card.suit)];
        sortedRanks[i] = card.rank;
    }

    std::sort(sortedRanks.begin(), sortedRanks.end(),
              std::greater<std::uint8_t>());

    const bool flush =
        std::any_of(suitCounts.begin(), suitCounts.end(),
                    [](int count) { return count == 5; });

    std::array<std::uint8_t, 5> uniqueRanks{};
    int uniqueCount = 0;
    for (int rank = kMaxRank; rank >= kMinRank; --rank) {
        if (rankCounts[rank] > 0) {
            uniqueRanks[uniqueCount++] = static_cast<std::uint8_t>(rank);
        }
    }

    int straightHigh = 0;
    if (uniqueCount == 5) {
        if (uniqueRanks[0] - uniqueRanks[4] == 4) {
            straightHigh = uniqueRanks[0];
        } else if (uniqueRanks[0] == 14 && uniqueRanks[1] == 5 &&
                   uniqueRanks[2] == 4 && uniqueRanks[3] == 3 &&
                   uniqueRanks[4] == 2) {
            straightHigh = 5;
        }
    }

    if (flush && straightHigh != 0) {
        if (straightHigh == 14) {
            return MakeRank(HandCategory::RoyalFlush, {14});
        }
        return MakeRank(HandCategory::StraightFlush, {straightHigh});
    }

    int fourOfAKind = 0;
    int threeOfAKind = 0;
    std::array<int, 2> pairs{};
    int pairCount = 0;
    std::array<int, 5> singles{};
    int singleCount = 0;

    for (int rank = kMaxRank; rank >= kMinRank; --rank) {
        switch (rankCounts[rank]) {
        case 4:
            fourOfAKind = rank;
            break;
        case 3:
            threeOfAKind = rank;
            break;
        case 2:
            if (pairCount < static_cast<int>(pairs.size())) {
                pairs[pairCount++] = rank;
            }
            break;
        case 1:
            if (singleCount < static_cast<int>(singles.size())) {
                singles[singleCount++] = rank;
            }
            break;
        default:
            break;
        }
    }

    if (fourOfAKind != 0) {
        return MakeRank(HandCategory::FourOfAKind,
                        {fourOfAKind, singles[0]});
    }

    if (threeOfAKind != 0 && pairCount > 0) {
        return MakeRank(HandCategory::FullHouse,
                        {threeOfAKind, pairs[0]});
    }

    if (flush) {
        return MakeRank(HandCategory::Flush,
                        {sortedRanks[0], sortedRanks[1], sortedRanks[2],
                         sortedRanks[3], sortedRanks[4]});
    }

    if (straightHigh != 0) {
        return MakeRank(HandCategory::Straight, {straightHigh});
    }

    if (threeOfAKind != 0) {
        return MakeRank(HandCategory::ThreeOfAKind,
                        {threeOfAKind, singles[0], singles[1]});
    }

    if (pairCount == 2) {
        return MakeRank(HandCategory::TwoPair,
                        {pairs[0], pairs[1], singles[0]});
    }

    if (pairCount == 1) {
        return MakeRank(HandCategory::OnePair,
                        {pairs[0], singles[0], singles[1], singles[2]});
    }

    return MakeRank(HandCategory::HighCard,
                    {sortedRanks[0], sortedRanks[1], sortedRanks[2],
                     sortedRanks[3], sortedRanks[4]});
}

} // namespace

common::Result<HandRank> EvaluateFive(const std::array<Card, 5>& cards) {
    bool seen[52] = {};
    for (const Card& card : cards) {
        const common::Status status = ValidateCard(card, seen);
        if (!status.ok()) {
            return status.error();
        }
    }
    return EvaluateFiveUnchecked(cards);
}

common::Result<HandRank> EvaluateHand(const std::vector<Card>& cards) {
    if (cards.size() < 5 || cards.size() > 7) {
        return common::MakeError(
            "invalid_hand_size",
            "expected 5 to 7 cards, got " + std::to_string(cards.size()));
    }

    bool seen[52] = {};
    for (const Card& card : cards) {
        const common::Status status = ValidateCard(card, seen);
        if (!status.ok()) {
            return status.error();
        }
    }

    HandRank best;
    bool haveBest = false;
    const std::size_t cardCount = cards.size();

    for (std::size_t a = 0; a + 4 < cardCount; ++a) {
        for (std::size_t b = a + 1; b + 3 < cardCount; ++b) {
            for (std::size_t c = b + 1; c + 2 < cardCount; ++c) {
                for (std::size_t d = c + 1; d + 1 < cardCount; ++d) {
                    for (std::size_t e = d + 1; e < cardCount; ++e) {
                        const std::array<Card, 5> five{
                            cards[a], cards[b], cards[c], cards[d],
                            cards[e]};
                        const HandRank current = EvaluateFiveUnchecked(five);
                        if (!haveBest || current > best) {
                            best = current;
                            haveBest = true;
                        }
                    }
                }
            }
        }
    }

    if (!haveBest) {
        return common::MakeError("invalid_hand_size",
                                 "no five-card combination found");
    }
    return best;
}

} // namespace poker::core
