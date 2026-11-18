#include <algorithm>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "common/result.h"
#include "poker_core/action.h"
#include "poker_core/action_validator.h"
#include "poker_core/deck.h"
#include "poker_core/game_engine.h"
#include "poker_core/game_state.h"
#include "poker_core/player.h"
#include "poker_core/rng.h"
#include "poker_core/table_config.h"

namespace {

using poker::core::Action;
using poker::core::ActionCandidate;
using poker::core::ActionKind;
using poker::core::GameEngine;
using poker::core::GameState;
using poker::core::LegalActions;
using poker::core::PlayerType;
using poker::core::Street;
using poker::core::TableConfig;

constexpr std::uint64_t IdForSeat(int seat) {
    return 1000 + static_cast<std::uint64_t>(seat);
}

TableConfig MakeConfig(std::uint64_t seed = 20261118) {
    TableConfig config;
    config.seat_count = 6;
    config.starting_stack = 1000;
    config.small_blind = 5;
    config.big_blind = 10;
    config.first_button_seat = 0;
    config.seed = seed;
    return config;
}

class EngineTest : public ::testing::Test {
protected:
    void Seat(GameEngine& engine, int seat, std::int64_t stack) {
        const auto status = engine.SeatPlayer(
            seat, IdForSeat(seat), "P" + std::to_string(seat),
            PlayerType::Rule, stack);
        ASSERT_TRUE(status.ok()) << status.error().code << ": "
                                 << status.error().message;
    }

    void Start(GameEngine& engine) {
        const auto status = engine.StartHand();
        ASSERT_TRUE(status.ok()) << status.error().code << ": "
                                 << status.error().message;
    }

    void Act(GameEngine& engine,
             int seat,
             ActionKind kind,
             std::int64_t target = 0) {
        const auto result =
            engine.ApplyAction(IdForSeat(seat), Action{kind, target});
        ASSERT_TRUE(result.ok()) << result.error().code << ": "
                                 << result.error().message;
    }

    poker::common::Result<LegalActions> Legal(GameEngine& engine, int seat) {
        return engine.GetLegalActions(IdForSeat(seat));
    }

    static bool HasKind(const LegalActions& legal, ActionKind kind) {
        for (const ActionCandidate& candidate : legal.candidates) {
            if (candidate.kind == kind) {
                return true;
            }
        }
        return false;
    }

    static void ExpectRoundInvariants(const GameState& state) {
        for (const auto& player : state.players) {
            EXPECT_GE(player.stack, 0);
            EXPECT_GE(player.committed_hand, 0);
            EXPECT_GE(player.committed_round, 0);
            EXPECT_LE(player.committed_round, state.current_bet);
            if (player.IsActionable() &&
                player.committed_round < state.current_bet) {
                EXPECT_TRUE(player.needs_action);
            }
        }
        EXPECT_LE(state.board.size(), 5u);
        EXPECT_LE(state.burn_cards.size(), 3u);
    }
};

TEST_F(EngineTest, HeadsUpBlindsDealAndFullHand) {
    GameEngine engine(MakeConfig());
    Seat(engine, 0, 1000);
    Seat(engine, 1, 1000);
    Start(engine);

    const GameState& state = engine.State();
    EXPECT_EQ(state.dealer_seat, 0);
    EXPECT_EQ(state.small_blind_seat, 0);
    EXPECT_EQ(state.big_blind_seat, 1);
    EXPECT_EQ(state.street, Street::Preflop);
    EXPECT_EQ(state.current_bet, 10);
    EXPECT_EQ(state.players[0].committed_round, 5);
    EXPECT_EQ(state.players[1].committed_round, 10);
    EXPECT_EQ(state.acting_seat, 0);
    EXPECT_EQ(state.TotalPot(), 15);

    ASSERT_EQ(state.players[0].hole_cards.size(), 2u);
    ASSERT_EQ(state.players[1].hole_cards.size(), 2u);
    std::set<std::string> dealt;
    for (const auto& player : state.players) {
        for (const auto& card : player.hole_cards) {
            EXPECT_TRUE(dealt.insert(poker::core::ToString(card)).second);
        }
    }
    EXPECT_EQ(dealt.size(), 4u);

    Act(engine, 0, ActionKind::Call);
    EXPECT_EQ(state.players[0].committed_round, 10);
    EXPECT_EQ(state.acting_seat, 1); // big blind option

    auto bigBlindLegal = Legal(engine, 1);
    ASSERT_TRUE(bigBlindLegal.ok()) << bigBlindLegal.error().message;
    EXPECT_TRUE(bigBlindLegal.value().can_check);
    EXPECT_TRUE(bigBlindLegal.value().can_raise);

    Act(engine, 1, ActionKind::Check);
    EXPECT_EQ(state.street, Street::Flop);
    ASSERT_EQ(state.board.size(), 3u);
    EXPECT_EQ(state.burn_cards.size(), 1u);
    EXPECT_EQ(state.acting_seat, 1); // heads-up: big blind acts first
    EXPECT_EQ(state.current_bet, 0);
    EXPECT_EQ(state.players[0].committed_round, 0);
    EXPECT_EQ(state.players[1].committed_round, 0);

    Act(engine, 1, ActionKind::Check);
    EXPECT_EQ(state.acting_seat, 0);
    Act(engine, 0, ActionKind::Check);
    EXPECT_EQ(state.street, Street::Turn);
    EXPECT_EQ(state.board.size(), 4u);
    EXPECT_EQ(state.burn_cards.size(), 2u);
    EXPECT_EQ(state.acting_seat, 1);

    Act(engine, 1, ActionKind::Check);
    Act(engine, 0, ActionKind::Check);
    EXPECT_EQ(state.street, Street::River);
    EXPECT_EQ(state.board.size(), 5u);
    EXPECT_EQ(state.burn_cards.size(), 3u);
    EXPECT_EQ(state.acting_seat, 1);

    Act(engine, 1, ActionKind::Check);
    EXPECT_EQ(state.acting_seat, 0);
    Act(engine, 0, ActionKind::Check);

    EXPECT_TRUE(state.hand_finished);
    EXPECT_EQ(state.street, Street::Showdown);
    EXPECT_FALSE(state.uncontested);
    EXPECT_TRUE(state.players[0].cards_visible);
    EXPECT_TRUE(state.players[1].cards_visible);
    EXPECT_EQ(state.TotalPot(), 20);
    EXPECT_EQ(engine.CountNonFolded(), 2);
}

TEST_F(EngineTest, ThreeHandedActionOrderAndButtonRotation) {
    GameEngine engine(MakeConfig());
    Seat(engine, 0, 1000);
    Seat(engine, 1, 1000);
    Seat(engine, 2, 1000);
    Start(engine);

    const GameState& state = engine.State();
    EXPECT_EQ(state.dealer_seat, 0);
    EXPECT_EQ(state.small_blind_seat, 1);
    EXPECT_EQ(state.big_blind_seat, 2);
    EXPECT_EQ(state.acting_seat, 0); // button acts first three-handed
    ExpectRoundInvariants(state);

    Act(engine, 0, ActionKind::Fold);
    EXPECT_EQ(state.acting_seat, 1);
    Act(engine, 1, ActionKind::Fold);

    EXPECT_TRUE(state.hand_finished);
    EXPECT_TRUE(state.uncontested);
    EXPECT_EQ(state.street, Street::Finished);
    EXPECT_EQ(state.board.size(), 0u);
    EXPECT_TRUE(state.burn_cards.empty());
    EXPECT_EQ(engine.CountNonFolded(), 1);
    EXPECT_FALSE(state.players[0].cards_visible);
    EXPECT_FALSE(state.players[1].cards_visible);

    Start(engine);
    EXPECT_EQ(state.dealer_seat, 1);
    EXPECT_EQ(state.small_blind_seat, 2);
    EXPECT_EQ(state.big_blind_seat, 0);
    EXPECT_EQ(state.acting_seat, 1);
}

TEST_F(EngineTest, BigBlindOptionAndFullRaiseReopensAction) {
    GameEngine engine(MakeConfig());
    Seat(engine, 0, 1000);
    Seat(engine, 1, 1000);
    Seat(engine, 2, 1000);
    Start(engine);

    Act(engine, 0, ActionKind::Call);
    Act(engine, 1, ActionKind::Call);

    const GameState& state = engine.State();
    EXPECT_EQ(state.acting_seat, 2);
    auto option = Legal(engine, 2);
    ASSERT_TRUE(option.ok()) << option.error().message;
    EXPECT_TRUE(option.value().can_check);
    EXPECT_TRUE(option.value().can_raise);

    Act(engine, 2, ActionKind::RaiseTo, 30);
    EXPECT_EQ(state.current_bet, 30);
    EXPECT_EQ(state.last_full_raise_to, 30);
    EXPECT_EQ(state.last_full_raise_size, 20);
    EXPECT_EQ(state.acting_seat, 0);
    EXPECT_FALSE(state.players[0].acted_since_full_raise);
    EXPECT_FALSE(state.players[1].acted_since_full_raise);

    auto response = Legal(engine, 0);
    ASSERT_TRUE(response.ok()) << response.error().message;
    EXPECT_TRUE(response.value().can_raise);
    EXPECT_EQ(response.value().to_call, 20);

    Act(engine, 0, ActionKind::Call);
    Act(engine, 1, ActionKind::Call);
    EXPECT_EQ(state.street, Street::Flop);
    EXPECT_EQ(state.current_bet, 0);
    EXPECT_EQ(state.last_full_raise_to, 0);
    EXPECT_EQ(state.last_full_raise_size, 10);
    ExpectRoundInvariants(state);
}

TEST_F(EngineTest, ShortAllInDoesNotReopenRaising) {
    GameEngine engine(MakeConfig());
    Seat(engine, 0, 1000);
    Seat(engine, 1, 1000);
    Seat(engine, 2, 240);
    Start(engine);

    Act(engine, 0, ActionKind::Call);
    Act(engine, 1, ActionKind::Call);
    Act(engine, 2, ActionKind::Check);

    const GameState& state = engine.State();
    ASSERT_EQ(state.street, Street::Flop);
    EXPECT_EQ(state.acting_seat, 1);

    Act(engine, 1, ActionKind::BetTo, 100);
    Act(engine, 2, ActionKind::Call);
    Act(engine, 0, ActionKind::Call);

    ASSERT_EQ(state.street, Street::Turn);
    Act(engine, 1, ActionKind::BetTo, 100);
    Act(engine, 2, ActionKind::AllIn);

    EXPECT_EQ(state.current_bet, 130);
    EXPECT_EQ(state.last_full_raise_to, 100);
    EXPECT_EQ(state.last_full_raise_size, 100);
    EXPECT_TRUE(state.players[2].all_in);
    EXPECT_EQ(state.acting_seat, 0);

    auto pending = Legal(engine, 0);
    ASSERT_TRUE(pending.ok()) << pending.error().message;
    EXPECT_TRUE(pending.value().can_raise);
    EXPECT_EQ(pending.value().min_raise_to, 230);

    Act(engine, 0, ActionKind::Call);
    EXPECT_EQ(state.acting_seat, 1);

    auto legal = Legal(engine, 1);
    ASSERT_TRUE(legal.ok()) << legal.error().message;
    EXPECT_EQ(legal.value().to_call, 30);
    EXPECT_FALSE(legal.value().can_raise);
    EXPECT_FALSE(HasKind(legal.value(), ActionKind::BetTo));
    EXPECT_FALSE(HasKind(legal.value(), ActionKind::RaiseTo));
    EXPECT_FALSE(HasKind(legal.value(), ActionKind::AllIn));

    const auto rejected =
        engine.ApplyAction(IdForSeat(1), Action{ActionKind::AllIn, 0});
    ASSERT_FALSE(rejected.ok());
    EXPECT_EQ(rejected.error().code, "no_raise_right");

    Act(engine, 1, ActionKind::Call);
    EXPECT_EQ(state.street, Street::River);
    EXPECT_EQ(state.board.size(), 5u);
    ExpectRoundInvariants(state);
}

TEST_F(EngineTest, CumulativeShortAllInsReopenRaising) {
    GameEngine engine(MakeConfig());
    Seat(engine, 0, 1000);
    Seat(engine, 1, 1000);
    Seat(engine, 2, 140);
    Seat(engine, 3, 210);
    Start(engine);

    // Four-handed preflop order: seat 3, seat 0, seat 1, seat 2.
    Act(engine, 3, ActionKind::Call);
    Act(engine, 0, ActionKind::Call);
    Act(engine, 1, ActionKind::Call);
    Act(engine, 2, ActionKind::Check);

    const GameState& state = engine.State();
    ASSERT_EQ(state.street, Street::Flop);
    EXPECT_EQ(state.acting_seat, 1);

    Act(engine, 1, ActionKind::BetTo, 100);
    Act(engine, 2, ActionKind::AllIn); // short raise: 100 -> 130
    Act(engine, 3, ActionKind::AllIn); // cumulative raise: 130 -> 200

    EXPECT_EQ(state.current_bet, 200);
    EXPECT_EQ(state.last_full_raise_to, 200);
    EXPECT_EQ(state.last_full_raise_size, 100);

    Act(engine, 0, ActionKind::Call);
    EXPECT_EQ(state.acting_seat, 1);
    EXPECT_FALSE(state.players[1].acted_since_full_raise);

    auto legal = Legal(engine, 1);
    ASSERT_TRUE(legal.ok()) << legal.error().message;
    EXPECT_TRUE(legal.value().can_raise);
    EXPECT_EQ(legal.value().to_call, 100);
    EXPECT_EQ(legal.value().min_raise_to, 300);
    EXPECT_TRUE(HasKind(legal.value(), ActionKind::RaiseTo));

    Act(engine, 1, ActionKind::RaiseTo, 300);
    EXPECT_EQ(state.current_bet, 300);
    EXPECT_EQ(state.acting_seat, 0);
    EXPECT_TRUE(state.players[0].needs_action);
    ExpectRoundInvariants(state);
}

TEST_F(EngineTest, CompletionOfShortFirstBetCountsAsFullBet) {
    GameEngine engine(MakeConfig());
    Seat(engine, 0, 1000);
    Seat(engine, 1, 1000);
    Seat(engine, 2, 14); // big blind leaves 4 chips
    Start(engine);

    Act(engine, 0, ActionKind::Call);
    Act(engine, 1, ActionKind::Call);
    Act(engine, 2, ActionKind::Check);

    const GameState& state = engine.State();
    ASSERT_EQ(state.street, Street::Flop);
    EXPECT_EQ(state.acting_seat, 1);

    Act(engine, 1, ActionKind::Check);
    Act(engine, 2, ActionKind::AllIn); // first bet is 4, below the big blind
    EXPECT_EQ(state.current_bet, 4);
    EXPECT_EQ(state.last_full_raise_to, 0);
    EXPECT_EQ(state.acting_seat, 0);

    auto legal = Legal(engine, 0);
    ASSERT_TRUE(legal.ok()) << legal.error().message;
    EXPECT_EQ(legal.value().min_raise_to, 10);
    EXPECT_TRUE(HasKind(legal.value(), ActionKind::RaiseTo));

    Act(engine, 0, ActionKind::RaiseTo, 10); // completion
    EXPECT_EQ(state.current_bet, 10);
    EXPECT_EQ(state.last_full_raise_to, 10);
    EXPECT_EQ(state.last_full_raise_size, 10);

    EXPECT_EQ(state.acting_seat, 1);
    auto next = Legal(engine, 1);
    ASSERT_TRUE(next.ok()) << next.error().message;
    EXPECT_EQ(next.value().min_raise_to, 20);
    EXPECT_TRUE(next.value().can_raise);
}

TEST_F(EngineTest, ShortBigBlindAndAllInRunout) {
    GameEngine engine(MakeConfig());
    Seat(engine, 0, 1000);
    Seat(engine, 1, 4);
    Start(engine);

    const GameState& state = engine.State();
    EXPECT_EQ(state.dealer_seat, 0);
    EXPECT_EQ(state.small_blind_seat, 0);
    EXPECT_EQ(state.big_blind_seat, 1);
    EXPECT_EQ(state.current_bet, 10);
    EXPECT_EQ(state.players[1].committed_hand, 4);
    EXPECT_TRUE(state.players[1].all_in);
    EXPECT_EQ(state.acting_seat, 0);

    auto legal = Legal(engine, 0);
    ASSERT_TRUE(legal.ok()) << legal.error().message;
    EXPECT_EQ(legal.value().to_call, 5);
    EXPECT_FALSE(legal.value().can_raise);
    EXPECT_TRUE(legal.value().can_call);
    EXPECT_FALSE(HasKind(legal.value(), ActionKind::RaiseTo));
    EXPECT_FALSE(HasKind(legal.value(), ActionKind::AllIn));

    Act(engine, 0, ActionKind::Call);

    EXPECT_TRUE(state.hand_finished);
    EXPECT_EQ(state.street, Street::Showdown);
    EXPECT_FALSE(state.uncontested);
    EXPECT_EQ(state.board.size(), 5u);
    EXPECT_EQ(state.burn_cards.size(), 3u);
    EXPECT_TRUE(state.players[0].cards_visible);
    EXPECT_TRUE(state.players[1].cards_visible);
    EXPECT_EQ(state.TotalPot(), 14);
    EXPECT_EQ(state.current_bet, 0);
}

TEST_F(EngineTest, ShortCallGoesAllInForLess) {
    GameEngine engine(MakeConfig());
    Seat(engine, 0, 8);
    Seat(engine, 1, 1000);
    Start(engine);

    const GameState& state = engine.State();
    EXPECT_EQ(state.small_blind_seat, 0);
    EXPECT_EQ(state.big_blind_seat, 1);
    EXPECT_EQ(state.players[0].committed_round, 5);
    EXPECT_EQ(state.players[0].stack, 3);
    EXPECT_EQ(state.acting_seat, 0);

    auto legal = Legal(engine, 0);
    ASSERT_TRUE(legal.ok()) << legal.error().message;
    EXPECT_EQ(legal.value().to_call, 5);
    EXPECT_TRUE(legal.value().can_call);
    EXPECT_FALSE(legal.value().can_raise);

    Act(engine, 0, ActionKind::Call);
    EXPECT_EQ(state.players[0].committed_hand, 8);
    EXPECT_EQ(state.players[0].stack, 0);
    EXPECT_TRUE(state.players[0].all_in);
    EXPECT_TRUE(state.hand_finished);
    EXPECT_EQ(state.street, Street::Showdown);
    EXPECT_EQ(state.board.size(), 5u);
    EXPECT_EQ(state.TotalPot(), 18);
}

TEST_F(EngineTest, LegalCandidatesAreUniqueAndSorted) {
    GameEngine engine(MakeConfig());
    Seat(engine, 0, 1000);
    Seat(engine, 1, 1000);
    Seat(engine, 2, 1000);
    Start(engine);

    auto legal = Legal(engine, 0);
    ASSERT_TRUE(legal.ok()) << legal.error().message;
    const LegalActions& actions = legal.value();
    EXPECT_EQ(actions.to_call, 10);
    EXPECT_EQ(actions.min_raise_to, 20);
    EXPECT_EQ(actions.max_raise_to, 1000);
    EXPECT_FALSE(actions.can_check);
    EXPECT_TRUE(actions.can_call);
    EXPECT_TRUE(actions.can_raise);
    ASSERT_FALSE(actions.candidates.empty());
    EXPECT_EQ(actions.candidates.front().kind, ActionKind::Fold);

    std::set<std::string> ids;
    std::vector<std::int64_t> raise_targets;
    bool saw_all_in = false;
    for (const ActionCandidate& candidate : actions.candidates) {
        EXPECT_TRUE(ids.insert(candidate.action_id).second);
        EXPECT_GE(candidate.amount, 0);
        if (candidate.kind == ActionKind::RaiseTo) {
            raise_targets.push_back(candidate.target_total);
        }
        if (candidate.kind == ActionKind::AllIn) {
            saw_all_in = true;
        }
    }
    EXPECT_EQ(ids.size(), actions.candidates.size());
    EXPECT_TRUE(saw_all_in);
    EXPECT_TRUE(std::is_sorted(raise_targets.begin(), raise_targets.end()));
    EXPECT_EQ(raise_targets.front(), actions.min_raise_to);

    const auto found =
        engine.FindCandidate(IdForSeat(0), actions.candidates.front().action_id);
    ASSERT_TRUE(found.ok()) << found.error().message;
    EXPECT_EQ(found.value().kind, ActionKind::Fold);

    const auto missing = engine.FindCandidate(IdForSeat(0), "missing");
    ASSERT_FALSE(missing.ok());
    EXPECT_EQ(missing.error().code, "candidate_not_found");
}

TEST_F(EngineTest, FixedSeedDealsIdenticalCards) {
    TableConfig config = MakeConfig(424242);
    GameEngine first(config);
    GameEngine second(config);
    for (int seat = 0; seat < 2; ++seat) {
        Seat(first, seat, 1000);
        Seat(second, seat, 1000);
    }
    Start(first);
    Start(second);

    EXPECT_EQ(first.State().hand_seed, second.State().hand_seed);
    for (int seat = 0; seat < 2; ++seat) {
        const auto& left = first.State().players[static_cast<std::size_t>(seat)]
                               .hole_cards;
        const auto& right =
            second.State().players[static_cast<std::size_t>(seat)].hole_cards;
        ASSERT_EQ(left.size(), right.size());
        for (std::size_t i = 0; i < left.size(); ++i) {
            EXPECT_EQ(poker::core::ToString(left[i]),
                      poker::core::ToString(right[i]));
        }
    }
}

TEST_F(EngineTest, FirstHandSkipsInactiveButtonSeat) {
    TableConfig config = MakeConfig();
    config.first_button_seat = 1;
    GameEngine engine(config);
    Seat(engine, 0, 1000);
    Seat(engine, 2, 1000);
    Seat(engine, 3, 1000);
    Start(engine);

    const GameState& state = engine.State();
    EXPECT_EQ(state.dealer_seat, 2);
    EXPECT_EQ(state.small_blind_seat, 3);
    EXPECT_EQ(state.big_blind_seat, 0);
    EXPECT_EQ(state.acting_seat, 2);
}

TEST_F(EngineTest, HoleCardsFollowClockwiseDealingOrder) {
    const auto expected_deal = [](const GameState& state,
                                  const std::vector<int>& order) {
        poker::core::Deck deck;
        poker::core::Rng rng(state.hand_seed);
        deck.Shuffle(rng);
        std::vector<std::string> expected;
        for (int round = 0; round < 2; ++round) {
            for (std::size_t i = 0; i < order.size(); ++i) {
                const auto card = deck.Draw();
                expected.push_back(poker::core::ToString(*card));
            }
        }
        return expected;
    };

    const auto actual_deal = [](const GameState& state,
                                const std::vector<int>& order) {
        std::vector<std::string> actual;
        for (int round = 0; round < 2; ++round) {
            for (const int seat : order) {
                actual.push_back(poker::core::ToString(
                    state.players[static_cast<std::size_t>(seat)]
                        .hole_cards[static_cast<std::size_t>(round)]));
            }
        }
        return actual;
    };

    {
        GameEngine heads_up(MakeConfig(31415));
        Seat(heads_up, 0, 1000);
        Seat(heads_up, 1, 1000);
        Start(heads_up);
        const std::vector<int> order = {1, 0}; // big blind first
        EXPECT_EQ(actual_deal(heads_up.State(), order),
                  expected_deal(heads_up.State(), order));
    }

    {
        GameEngine three_handed(MakeConfig(27182));
        Seat(three_handed, 0, 1000);
        Seat(three_handed, 1, 1000);
        Seat(three_handed, 2, 1000);
        Start(three_handed);
        const std::vector<int> order = {1, 2, 0}; // SB, BB, button
        EXPECT_EQ(actual_deal(three_handed.State(), order),
                  expected_deal(three_handed.State(), order));
    }
}

TEST_F(EngineTest, DeterministicRandomPlayMaintainsInvariants) {
    GameEngine engine(MakeConfig(777));
    for (int seat = 0; seat < 6; ++seat) {
        Seat(engine, seat, 1000);
    }

    poker::core::Rng chooser(999);
    int completed_hands = 0;
    for (int hand = 0; hand < 12; ++hand) {
        const auto started = engine.StartHand();
        if (!started.ok()) {
            EXPECT_EQ(started.error().code, "not_enough_players");
            break;
        }

        const GameState& state = engine.State();
        std::int64_t hand_start_chips = 0;
        for (const auto& player : state.players) {
            hand_start_chips += player.stack + player.committed_hand;
        }
        EXPECT_GT(hand_start_chips, 0);

        int actions = 0;
        while (!state.hand_finished) {
            ASSERT_LT(++actions, 300) << "hand did not terminate";
            ASSERT_GE(state.acting_seat, 0);
            ASSERT_LT(state.acting_seat, static_cast<int>(state.players.size()));
            const auto& actor =
                state.players[static_cast<std::size_t>(state.acting_seat)];
            auto legal = engine.GetLegalActions(actor.id);
            ASSERT_TRUE(legal.ok()) << legal.error().code << ": "
                                    << legal.error().message;
            ASSERT_FALSE(legal.value().candidates.empty());

            const auto& candidate = legal.value().candidates
                [chooser.UniformIndex(legal.value().candidates.size())];
            const auto applied =
                engine.ApplyAction(actor.id,
                                   Action{candidate.kind,
                                          candidate.target_total});
            ASSERT_TRUE(applied.ok()) << applied.error().code << ": "
                                      << applied.error().message;

            std::int64_t after = 0;
            for (const auto& player : state.players) {
                EXPECT_GE(player.stack, 0);
                EXPECT_GE(player.committed_hand, 0);
                after += player.stack + player.committed_hand;
            }
            EXPECT_EQ(after, hand_start_chips);
            EXPECT_LE(state.board.size(), 5u);
            EXPECT_LE(state.burn_cards.size(), 3u);
        }

        EXPECT_TRUE(state.street == Street::Showdown ||
                    state.street == Street::Finished);
        std::set<std::string> seen;
        for (const auto& player : state.players) {
            for (const auto& card : player.hole_cards) {
                EXPECT_TRUE(seen.insert(poker::core::ToString(card)).second);
            }
        }
        for (const auto& card : state.board) {
            EXPECT_TRUE(seen.insert(poker::core::ToString(card)).second);
        }
        for (const auto& card : state.burn_cards) {
            EXPECT_TRUE(seen.insert(poker::core::ToString(card)).second);
        }
        ++completed_hands;
    }

    EXPECT_GT(completed_hands, 0);
}

TEST_F(EngineTest, ShortSmallBlindAllInEndsBettingImmediately) {
    GameEngine engine(MakeConfig());
    Seat(engine, 0, 3);
    Seat(engine, 1, 1000);
    Start(engine);

    const GameState& state = engine.State();
    EXPECT_EQ(state.small_blind_seat, 0);
    EXPECT_EQ(state.big_blind_seat, 1);
    EXPECT_TRUE(state.players[0].all_in);
    EXPECT_EQ(state.players[0].committed_hand, 3);
    EXPECT_EQ(state.players[1].committed_hand, 10);
    EXPECT_TRUE(state.hand_finished);
    EXPECT_EQ(state.street, Street::Showdown);
    EXPECT_EQ(state.board.size(), 5u);
    EXPECT_EQ(state.TotalPot(), 13);
}

TEST_F(EngineTest, BothBlindsAllInTriggerImmediateRunout) {
    GameEngine engine(MakeConfig());
    Seat(engine, 0, 5);
    Seat(engine, 1, 5);
    Start(engine);

    const GameState& state = engine.State();
    EXPECT_TRUE(state.players[0].all_in);
    EXPECT_TRUE(state.players[1].all_in);
    EXPECT_EQ(state.players[0].committed_hand, 5);
    EXPECT_EQ(state.players[1].committed_hand, 5);
    EXPECT_TRUE(state.hand_finished);
    EXPECT_EQ(state.street, Street::Showdown);
    EXPECT_EQ(state.board.size(), 5u);
    EXPECT_EQ(state.burn_cards.size(), 3u);
    EXPECT_TRUE(state.players[0].cards_visible);
    EXPECT_TRUE(state.players[1].cards_visible);
    EXPECT_EQ(engine.CountActionable(), 0);
    EXPECT_EQ(state.TotalPot(), 10);
}

TEST_F(EngineTest, ActionValidationErrors) {
    GameEngine engine(MakeConfig());
    Seat(engine, 0, 1000);
    Seat(engine, 1, 1000);
    Seat(engine, 2, 1000);
    Start(engine);

    auto outOfTurn = engine.ApplyAction(IdForSeat(1),
                                        Action{ActionKind::Call, 0});
    ASSERT_FALSE(outOfTurn.ok());
    EXPECT_EQ(outOfTurn.error().code, "not_your_turn");

    auto badCheck = engine.ApplyAction(IdForSeat(0),
                                       Action{ActionKind::Check, 0});
    ASSERT_FALSE(badCheck.ok());
    EXPECT_EQ(badCheck.error().code, "check_facing_bet");

    auto tooSmall = engine.ApplyAction(IdForSeat(0),
                                       Action{ActionKind::RaiseTo, 15});
    ASSERT_FALSE(tooSmall.ok());
    EXPECT_EQ(tooSmall.error().code, "target_too_small");

    auto tooLarge = engine.ApplyAction(IdForSeat(0),
                                       Action{ActionKind::RaiseTo, 5000});
    ASSERT_FALSE(tooLarge.ok());
    EXPECT_EQ(tooLarge.error().code, "target_too_large");

    auto wrongKind = engine.ApplyAction(IdForSeat(0),
                                        Action{ActionKind::BetTo, 20});
    ASSERT_FALSE(wrongKind.ok());
    EXPECT_EQ(wrongKind.error().code, "bet_requires_no_bet");

    auto seatDuringHand = engine.SeatPlayer(3, 1003, "P3", PlayerType::Rule,
                                            1000);
    ASSERT_FALSE(seatDuringHand.ok());
    EXPECT_EQ(seatDuringHand.error().code, "hand_in_progress");

    Act(engine, 0, ActionKind::Call);
    Act(engine, 1, ActionKind::Call);
    Act(engine, 2, ActionKind::Check);
    EXPECT_EQ(engine.State().street, Street::Flop);

    Act(engine, 1, ActionKind::Check);
    Act(engine, 2, ActionKind::Check);
    Act(engine, 0, ActionKind::Check);
    EXPECT_EQ(engine.State().street, Street::Turn);
}

TEST_F(EngineTest, SittingOutSkipsSeatAndCreatesHeadsUp) {
    GameEngine engine(MakeConfig());
    Seat(engine, 0, 1000);
    Seat(engine, 1, 1000);
    Seat(engine, 2, 1000);
    Start(engine);

    Act(engine, 0, ActionKind::Fold);
    Act(engine, 1, ActionKind::Fold);
    EXPECT_TRUE(engine.State().hand_finished);

    const auto sitOut = engine.SetSittingOut(IdForSeat(1), true);
    ASSERT_TRUE(sitOut.ok()) << sitOut.error().message;

    Start(engine);
    const GameState& state = engine.State();
    EXPECT_EQ(engine.CountActionable(), 2);
    EXPECT_EQ(state.dealer_seat, 2);
    EXPECT_EQ(state.small_blind_seat, 2);
    EXPECT_EQ(state.big_blind_seat, 0);
    EXPECT_EQ(state.acting_seat, 2);
}

TEST_F(EngineTest, TableConfigAndSeatingValidation) {
    TableConfig bad = MakeConfig();
    bad.seat_count = 1;
    EXPECT_THROW(GameEngine{bad}, std::invalid_argument);

    bad = MakeConfig();
    bad.first_button_seat = 6;
    EXPECT_THROW(GameEngine{bad}, std::invalid_argument);

    bad = MakeConfig();
    bad.small_blind = 20;
    bad.big_blind = 10;
    EXPECT_THROW(GameEngine{bad}, std::invalid_argument);

    GameEngine engine(MakeConfig());
    EXPECT_TRUE(engine.SeatPlayer(0, 1, "A", PlayerType::Human, 500).ok());
    EXPECT_EQ(engine.SeatPlayer(0, 2, "B", PlayerType::Human, 500).error().code,
              "seat_occupied");
    EXPECT_EQ(engine.SeatPlayer(1, 1, "C", PlayerType::Human, 500).error().code,
              "duplicate_player_id");
    EXPECT_EQ(engine.SeatPlayer(6, 3, "D", PlayerType::Human, 500).error().code,
              "invalid_seat");

    const auto notEnough = engine.StartHand();
    ASSERT_FALSE(notEnough.ok());
    EXPECT_EQ(notEnough.error().code, "not_enough_players");
}

} // namespace
