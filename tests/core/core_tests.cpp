#include <array>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "common/result.h"
#include "poker_core/card.h"
#include "poker_core/deck.h"
#include "poker_core/hand_evaluator.h"
#include "poker_core/hand_rank.h"
#include "poker_core/rng.h"

namespace {

using poker::common::Result;
using poker::core::Card;
using poker::core::Deck;
using poker::core::EvaluateFive;
using poker::core::EvaluateHand;
using poker::core::HandCategory;
using poker::core::HandRank;
using poker::core::Rng;
using poker::core::Suit;

std::vector<Card> ParseOrThrow(std::string_view text) {
    auto cards = poker::core::ParseCards(text);
    if (!cards.ok()) {
        throw std::runtime_error(cards.error().message);
    }
    return cards.value();
}

HandRank RankOf(std::string_view text) {
    auto rank = EvaluateHand(ParseOrThrow(text));
    if (!rank.ok()) {
        throw std::runtime_error(rank.error().message);
    }
    return rank.value();
}

TEST(CardTest, ParseFormatAndValidity) {
    const auto aceSpades = poker::core::ParseCard("As");
    ASSERT_TRUE(aceSpades.has_value());
    EXPECT_EQ(aceSpades->rank, 14);
    EXPECT_EQ(aceSpades->suit, Suit::Spades);
    EXPECT_EQ(poker::core::ToString(*aceSpades), "As");
    EXPECT_TRUE(aceSpades->IsValid());

    const auto tenHearts = poker::core::ParseCard("10h");
    ASSERT_TRUE(tenHearts.has_value());
    EXPECT_EQ(tenHearts->rank, 10);
    EXPECT_EQ(tenHearts->suit, Suit::Hearts);
    EXPECT_EQ(poker::core::ToString(*tenHearts), "Th");

    const auto tenDiamonds = poker::core::ParseCard("td");
    ASSERT_TRUE(tenDiamonds.has_value());
    EXPECT_EQ(tenDiamonds->rank, 10);
    EXPECT_EQ(tenDiamonds->suit, Suit::Diamonds);
    EXPECT_NE(*tenHearts, *tenDiamonds);

    EXPECT_FALSE(poker::core::ParseCard("1s").has_value());
    EXPECT_FALSE(poker::core::ParseCard("Ax").has_value());
    EXPECT_FALSE(poker::core::ParseCard("").has_value());
    EXPECT_FALSE(poker::core::ParseCard("Asd").has_value());
    EXPECT_FALSE((Card{1, Suit::Spades}.IsValid()));
    EXPECT_FALSE((Card{15, Suit::Spades}.IsValid()));
    EXPECT_FALSE((Card{2, static_cast<Suit>(9)}.IsValid()));
}

TEST(CardTest, IndexRoundTrip) {
    std::set<int> indexes;
    for (int index = 0; index < 52; ++index) {
        const auto card = poker::core::CardFromIndex(index);
        ASSERT_TRUE(card.has_value()) << index;
        EXPECT_TRUE(card->IsValid());
        EXPECT_EQ(poker::core::CardIndex(*card), index);
        indexes.insert(index);
    }
    EXPECT_EQ(indexes.size(), 52u);
    EXPECT_FALSE(poker::core::CardFromIndex(-1).has_value());
    EXPECT_FALSE(poker::core::CardFromIndex(52).has_value());
    EXPECT_EQ(poker::core::CardIndex(Card{1, Suit::Clubs}), -1);
}

TEST(CardTest, ParseCards) {
    const auto cards = poker::core::ParseCards("As Kd 10h 2c");
    ASSERT_TRUE(cards.ok()) << cards.error().message;
    ASSERT_EQ(cards.value().size(), 4u);
    EXPECT_EQ(poker::core::ToString(cards.value()[0]), "As");
    EXPECT_EQ(poker::core::ToString(cards.value()[2]), "Th");

    const auto bad = poker::core::ParseCards("As XX");
    ASSERT_FALSE(bad.ok());
    EXPECT_EQ(bad.error().code, "invalid_card");
}

TEST(RngTest, DeterministicSequences) {
    Rng left(42);
    Rng right(42);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(left.Next(), right.Next());
    }

    Rng different(43);
    Rng same(42);
    std::array<std::uint64_t, 4> differentValues{};
    std::array<std::uint64_t, 4> sameValues{};
    for (std::size_t i = 0; i < differentValues.size(); ++i) {
        differentValues[i] = different.Next();
        sameValues[i] = same.Next();
    }
    EXPECT_NE(differentValues, sameValues);
}

TEST(RngTest, UniformRanges) {
    Rng rng(7);
    for (int i = 0; i < 1000; ++i) {
        const int die = rng.UniformInt(1, 6);
        EXPECT_GE(die, 1);
        EXPECT_LE(die, 6);

        const std::size_t index = rng.UniformIndex(52);
        EXPECT_LT(index, 52u);

        const std::uint64_t value = rng.UniformUInt64(10);
        EXPECT_LT(value, 10u);
    }

    EXPECT_EQ(rng.UniformInt(5, 5), 5);
    EXPECT_THROW(rng.UniformUInt64(0), std::invalid_argument);
    EXPECT_THROW(rng.UniformIndex(0), std::invalid_argument);
    EXPECT_THROW(rng.UniformInt(2, 1), std::invalid_argument);
}

TEST(DeckTest, ResetAndDrawAll) {
    Deck deck;
    EXPECT_EQ(deck.Remaining(), Deck::kCardCount);
    EXPECT_FALSE(deck.Empty());
    EXPECT_EQ(poker::core::ToString(deck.Cards().front()), "2c");
    EXPECT_EQ(poker::core::ToString(deck.Cards().back()), "As");

    std::set<int> seen;
    for (std::size_t i = 0; i < Deck::kCardCount; ++i) {
        const auto card = deck.Draw();
        ASSERT_TRUE(card.has_value()) << i;
        EXPECT_TRUE(card->IsValid());
        EXPECT_TRUE(seen.insert(poker::core::CardIndex(*card)).second);
    }
    EXPECT_EQ(seen.size(), Deck::kCardCount);
    EXPECT_TRUE(deck.Empty());
    EXPECT_EQ(deck.Remaining(), 0u);
    EXPECT_FALSE(deck.Draw().has_value());
}

TEST(DeckTest, DeterministicShuffle) {
    Deck left;
    Deck right;
    Deck different;

    Rng leftRng(123456);
    Rng rightRng(123456);
    Rng differentRng(654321);

    left.Shuffle(leftRng);
    right.Shuffle(rightRng);
    different.Shuffle(differentRng);

    EXPECT_EQ(left.Cards(), right.Cards());
    EXPECT_NE(left.Cards(), different.Cards());

    std::set<int> seen;
    for (std::size_t i = 0; i < Deck::kCardCount; ++i) {
        const auto card = left.Draw();
        ASSERT_TRUE(card.has_value());
        EXPECT_TRUE(seen.insert(poker::core::CardIndex(*card)).second);
    }
    EXPECT_EQ(seen.size(), Deck::kCardCount);
}

TEST(HandEvaluatorTest, Categories) {
    struct TestCase {
        const char* cards;
        HandCategory category;
    };

    const TestCase cases[] = {
        {"As Jd 9h 6s 2c", HandCategory::HighCard},
        {"Tc Td As 8h 3c", HandCategory::OnePair},
        {"Jc Jd 4h 4s As", HandCategory::TwoPair},
        {"7c 7d 7h Ks 2c", HandCategory::ThreeOfAKind},
        {"9c 8d 7h 6s 5c", HandCategory::Straight},
        {"As Js 8s 4s 2s", HandCategory::Flush},
        {"8c 8d 8h 3s 3c", HandCategory::FullHouse},
        {"9c 9d 9h 9s 2c", HandCategory::FourOfAKind},
        {"9h 8h 7h 6h 5h", HandCategory::StraightFlush},
        {"As Ks Qs Js Ts", HandCategory::RoyalFlush},
    };

    for (const TestCase& test : cases) {
        SCOPED_TRACE(test.cards);
        const HandRank rank = RankOf(test.cards);
        EXPECT_EQ(rank.category, test.category);
    }
}

TEST(HandEvaluatorTest, TiebreakValues) {
    const auto highCard = RankOf("As Jd 9h 6s 2c");
    EXPECT_EQ(highCard.tiebreak_count, 5);
    EXPECT_EQ(highCard.tiebreak[0], 14);
    EXPECT_EQ(highCard.tiebreak[4], 2);

    const auto pair = RankOf("Ac Ad Ks 8h 3c");
    EXPECT_EQ(pair.tiebreak_count, 4);
    EXPECT_EQ(pair.tiebreak[0], 14);
    EXPECT_EQ(pair.tiebreak[1], 13);
    EXPECT_EQ(pair.tiebreak[3], 3);

    const auto twoPair = RankOf("Jc Jd 4h 4s As");
    EXPECT_EQ(twoPair.tiebreak_count, 3);
    EXPECT_EQ(twoPair.tiebreak[0], 11);
    EXPECT_EQ(twoPair.tiebreak[1], 4);
    EXPECT_EQ(twoPair.tiebreak[2], 14);

    const auto trips = RankOf("7c 7d 7h Ks 2c");
    EXPECT_EQ(trips.tiebreak_count, 3);
    EXPECT_EQ(trips.tiebreak[0], 7);
    EXPECT_EQ(trips.tiebreak[1], 13);
    EXPECT_EQ(trips.tiebreak[2], 2);

    const auto straight = RankOf("9c 8d 7h 6s 5c");
    EXPECT_EQ(straight.tiebreak_count, 1);
    EXPECT_EQ(straight.tiebreak[0], 9);

    const auto wheel = RankOf("5c 4d 3h 2s Ac");
    EXPECT_EQ(wheel.category, HandCategory::Straight);
    EXPECT_EQ(wheel.tiebreak[0], 5);

    const auto flush = RankOf("As Js 8s 4s 2s");
    EXPECT_EQ(flush.tiebreak_count, 5);
    EXPECT_EQ(flush.tiebreak[0], 14);
    EXPECT_EQ(flush.tiebreak[1], 11);
    EXPECT_EQ(flush.tiebreak[4], 2);

    const auto fullHouse = RankOf("8c 8d 8h 3s 3c");
    EXPECT_EQ(fullHouse.tiebreak_count, 2);
    EXPECT_EQ(fullHouse.tiebreak[0], 8);
    EXPECT_EQ(fullHouse.tiebreak[1], 3);

    const auto quads = RankOf("9c 9d 9h 9s 2c");
    EXPECT_EQ(quads.tiebreak_count, 2);
    EXPECT_EQ(quads.tiebreak[0], 9);
    EXPECT_EQ(quads.tiebreak[1], 2);
}

TEST(HandEvaluatorTest, KickerComparisons) {
    EXPECT_GT(RankOf("Ac Ad Ks Qh 2c"), RankOf("Ac Ad Ks Jh 2c"));
    EXPECT_GT(RankOf("Ac Ad Ks Kh 2c"), RankOf("Ac Ad Ks Qh 2c"));
    EXPECT_GT(RankOf("7c 7d 7h As 2c"), RankOf("7c 7d 7h Ks Qc"));
    EXPECT_GT(RankOf("9c 9d 9h 9s Ac"), RankOf("9c 9d 9h 9s Kc"));
    EXPECT_GT(RankOf("8c 8d 8h 3s 3c"), RankOf("7c 7d 7h As Ac"));
    EXPECT_LT(RankOf("As Js 8s 4s 2s"), RankOf("As Js 8s 4s 3s"));
    EXPECT_GT(RankOf("6c 5d 4h 3s 2c"), RankOf("5c 4d 3h 2s Ac"));
    EXPECT_LT(RankOf("Ac Ad Ks Qh 2c"), RankOf("Ac Ad Ks Qh 3c"));
}

TEST(HandEvaluatorTest, CrossCategoryOrdering) {
    EXPECT_GT(RankOf("As Ks Qs Js Ts"), RankOf("9h 8h 7h 6h 5h"));
    EXPECT_GT(RankOf("9h 8h 7h 6h 5h"), RankOf("9c 9d 9h 9s 2c"));
    EXPECT_GT(RankOf("9c 9d 9h 9s 2c"), RankOf("8c 8d 8h 3s 3c"));
    EXPECT_GT(RankOf("8c 8d 8h 3s 3c"), RankOf("As Js 8s 4s 2s"));
    EXPECT_GT(RankOf("As Js 8s 4s 2s"), RankOf("9c 8d 7h 6s 5c"));
    EXPECT_GT(RankOf("9c 8d 7h 6s 5c"), RankOf("7c 7d 7h Ks 2c"));
    EXPECT_GT(RankOf("7c 7d 7h Ks 2c"), RankOf("Jc Jd 4h 4s As"));
    EXPECT_GT(RankOf("Jc Jd 4h 4s As"), RankOf("Tc Td As 8h 3c"));
    EXPECT_GT(RankOf("Tc Td As 8h 3c"), RankOf("As Jd 9h 6s 2c"));
}

TEST(HandEvaluatorTest, SameRanksAreTies) {
    EXPECT_EQ(RankOf("9c 8d 7h 6s 5c"), RankOf("9h 8c 7d 6c 5h"));
    EXPECT_EQ(RankOf("As Ks Qs Js Ts"), RankOf("Ah Kh Qh Jh Th"));
}

TEST(HandEvaluatorTest, SixAndSevenCardBestHand) {
    const auto sixCardRoyal = RankOf("As Ks Qs Js Ts 2c");
    EXPECT_EQ(sixCardRoyal.category, HandCategory::RoyalFlush);

    const auto royal = RankOf("As Ks Qs Js Ts 2c 3d");
    EXPECT_EQ(royal.category, HandCategory::RoyalFlush);

    const auto quads = RankOf("9c 9d 9h 9s 2c 3d 4h");
    EXPECT_EQ(quads.category, HandCategory::FourOfAKind);
    EXPECT_EQ(quads.tiebreak[0], 9);
    EXPECT_EQ(quads.tiebreak[1], 4);

    const auto fullHouse = RankOf("8c 8d 8h 3s 3c 2d 2h");
    EXPECT_EQ(fullHouse.category, HandCategory::FullHouse);
    EXPECT_EQ(fullHouse.tiebreak[0], 8);
    EXPECT_EQ(fullHouse.tiebreak[1], 3);

    const auto flush = RankOf("Ah Kh Qh Jh 9h 2c 3d");
    EXPECT_EQ(flush.category, HandCategory::Flush);
    EXPECT_EQ(flush.tiebreak[0], 14);
    EXPECT_EQ(flush.tiebreak[4], 9);

    const auto wheel = RankOf("5c 4d 3h 2s Ac Kd Qh");
    EXPECT_EQ(wheel.category, HandCategory::Straight);
    EXPECT_EQ(wheel.tiebreak[0], 5);
}

TEST(HandEvaluatorTest, InvalidInputs) {
    EXPECT_FALSE(EvaluateHand(ParseOrThrow("As Ks Qs Js")).ok());
    EXPECT_FALSE(
        EvaluateHand(ParseOrThrow("As Ks Qs Js Ts 9s 8s 7s")).ok());

    const auto duplicate =
        EvaluateHand(ParseOrThrow("As As Ks Qs Js"));
    ASSERT_FALSE(duplicate.ok());
    EXPECT_EQ(duplicate.error().code, "duplicate_card");

    const auto invalidRank = EvaluateHand(
        std::vector<Card>{Card{1, Suit::Spades}, Card{2, Suit::Clubs},
                          Card{3, Suit::Diamonds}, Card{4, Suit::Hearts},
                          Card{5, Suit::Spades}});
    ASSERT_FALSE(invalidRank.ok());
    EXPECT_EQ(invalidRank.error().code, "invalid_card");

    const auto invalidSuit = EvaluateHand(
        std::vector<Card>{Card{2, static_cast<Suit>(9)},
                          Card{3, Suit::Clubs}, Card{4, Suit::Diamonds},
                          Card{5, Suit::Hearts}, Card{6, Suit::Spades}});
    ASSERT_FALSE(invalidSuit.ok());
    EXPECT_EQ(invalidSuit.error().code, "invalid_card");
}

TEST(HandEvaluatorTest, EvaluateFiveApi) {
    const std::array<Card, 5> cards{
        Card{14, Suit::Spades}, Card{13, Suit::Spades},
        Card{12, Suit::Spades}, Card{11, Suit::Spades},
        Card{10, Suit::Spades}};

    const auto rank = EvaluateFive(cards);
    ASSERT_TRUE(rank.ok()) << rank.error().message;
    EXPECT_EQ(rank.value().category, HandCategory::RoyalFlush);
}

TEST(HandRankTest, Formatting) {
    const HandRank rank = RankOf("8c 8d 8h 3s 3c");
    EXPECT_EQ(poker::core::ToString(rank), "Full House (8, 3)");
}

} // namespace
