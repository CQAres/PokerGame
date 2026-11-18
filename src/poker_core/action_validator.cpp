#include "poker_core/action_validator.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <utility>

namespace poker::core {
namespace {

void AddCandidate(LegalActions& legal,
                  ActionKind kind,
                  std::int64_t amount,
                  std::int64_t target_total,
                  bool is_all_in,
                  std::string label) {
    ActionCandidate candidate;
    candidate.action_id = "a" + std::to_string(legal.candidates.size() + 1);
    candidate.kind = kind;
    candidate.amount = amount;
    candidate.target_total = target_total;
    candidate.min_raise_to = legal.min_raise_to;
    candidate.max_raise_to = legal.max_raise_to;
    candidate.is_all_in = is_all_in;
    candidate.label = std::move(label);
    legal.candidates.push_back(std::move(candidate));
}

std::string AmountLabel(const char* verb, std::int64_t target) {
    return std::string(verb) + " " + std::to_string(target);
}

} // namespace

std::int64_t ToCall(const GameState& state, const Player& player) noexcept {
    return std::max<std::int64_t>(0, state.current_bet - player.committed_round);
}

std::int64_t MaxTarget(const Player& player) noexcept {
    return player.committed_round + player.stack;
}

std::int64_t ComputeMinRaiseTo(const GameState& state) noexcept {
    if (state.current_bet == 0) {
        return state.big_blind;
    }

    // The first bet of a round was a short all-in. A later player may call
    // that amount or complete the bet to a full big blind.
    if (state.current_bet < state.big_blind && state.last_full_raise_to == 0) {
        return state.big_blind;
    }

    const std::int64_t raise_size =
        state.last_full_raise_size > 0 ? state.last_full_raise_size
                                       : state.big_blind;
    return state.current_bet + raise_size;
}

int CountActionableOpponents(const GameState& state, int seat) noexcept {
    int count = 0;
    for (const Player& player : state.players) {
        if (player.seat != seat && player.IsActionable()) {
            ++count;
        }
    }
    return count;
}

bool CanPlayerRaise(const GameState& state, const Player& player) noexcept {
    if (!player.IsActionable()) {
        return false;
    }
    const std::int64_t to_call = ToCall(state, player);
    if (player.stack <= to_call) {
        return false;
    }
    if (player.acted_since_full_raise) {
        return false;
    }
    return CountActionableOpponents(state, player.seat) >= 1;
}

LegalActions BuildLegalActions(const GameState& state, const Player& player) {
    LegalActions legal;
    legal.to_call = ToCall(state, player);
    legal.min_raise_to = ComputeMinRaiseTo(state);
    legal.max_raise_to = MaxTarget(player);
    legal.can_check = legal.to_call == 0;
    legal.can_call = legal.to_call > 0;
    legal.can_raise = CanPlayerRaise(state, player);

    AddCandidate(legal, ActionKind::Fold, 0, player.committed_round, false,
                 "Fold");

    if (legal.can_check) {
        AddCandidate(legal, ActionKind::Check, 0, player.committed_round,
                     false, "Check");
    } else {
        const std::int64_t call_amount =
            std::min(legal.to_call, player.stack);
        AddCandidate(legal, ActionKind::Call, call_amount,
                     player.committed_round + call_amount,
                     call_amount >= player.stack, "Call " +
                                                     std::to_string(call_amount));
    }

    if (legal.can_raise && legal.max_raise_to >= legal.min_raise_to) {
        std::set<std::int64_t> targets;
        targets.insert(legal.min_raise_to);

        const std::int64_t pot_after_call = state.TotalPot() + legal.to_call;
        const std::int64_t numerators[] = {1, 3, 2, 4};
        const std::int64_t denominators[] = {2, 4, 2, 1};
        for (std::size_t i = 0; i < 4; ++i) {
            const std::int64_t raise_by =
                pot_after_call * numerators[i] / denominators[i];
            const std::int64_t target =
                std::max(state.current_bet, std::int64_t{0}) + raise_by;
            targets.insert(target);
        }

        const ActionKind kind = state.current_bet == 0 ? ActionKind::BetTo
                                                       : ActionKind::RaiseTo;
        for (const std::int64_t target : targets) {
            if (target <= state.current_bet ||
                target < legal.min_raise_to ||
                target >= legal.max_raise_to) {
                // The exact maximum is offered through the explicit AllIn
                // candidate below, which also covers short all-ins.
                continue;
            }
            AddCandidate(legal, kind, target - player.committed_round, target,
                         false,
                         AmountLabel(kind == ActionKind::BetTo ? "Bet to"
                                                               : "Raise to",
                                     target));
        }
    }

    if (player.stack > 0) {
        const std::int64_t all_in_target = legal.max_raise_to;
        const bool is_raise = all_in_target > state.current_bet;
        if (!is_raise || legal.can_raise) {
            AddCandidate(
                legal, ActionKind::AllIn, player.stack, all_in_target, true,
                std::string("All-in ") + std::to_string(all_in_target));
        }
    }

    return legal;
}

common::Status ActionValidator::Validate(const GameState& state,
                                         const Player& player,
                                         const LegalActions& legal,
                                         const Action& action) {
    if (state.hand_finished || !IsBettingStreet(state.street)) {
        return common::MakeError("invalid_state",
                                 "no betting action is possible now");
    }
    if (state.acting_seat != player.seat) {
        return common::MakeError("not_your_turn",
                                 "player is not the acting player");
    }
    if (!player.IsActionable()) {
        return common::MakeError("player_not_actionable",
                                 "player cannot act in the current hand");
    }

    const std::int64_t to_call = ToCall(state, player);
    const std::int64_t max_target = MaxTarget(player);
    const std::int64_t min_raise_to = ComputeMinRaiseTo(state);

    switch (action.kind) {
    case ActionKind::Fold:
        return common::Status::Ok();

    case ActionKind::Check:
        if (to_call != 0) {
            return common::MakeError("check_facing_bet",
                                     "cannot check while facing a bet");
        }
        return common::Status::Ok();

    case ActionKind::Call:
        if (to_call <= 0) {
            return common::MakeError("nothing_to_call",
                                     "cannot call when to_call is zero");
        }
        return common::Status::Ok();

    case ActionKind::BetTo: {
        if (state.current_bet != 0) {
            return common::MakeError("bet_requires_no_bet",
                                     "BetTo requires an empty current bet");
        }
        if (!legal.can_raise) {
            return common::MakeError(
                "no_raise_right",
                "player may not bet or raise in this situation");
        }

        const std::int64_t target = action.target_total;
        if (target <= state.current_bet) {
            return common::MakeError(
                "target_not_above_current_bet",
                "bet target must be greater than the current bet");
        }
        if (target > max_target) {
            return common::MakeError("target_too_large",
                                     "bet target exceeds the player's stack");
        }

        const bool is_all_in = target == max_target;
        if (target < state.big_blind && !is_all_in) {
            return common::MakeError(
                "target_too_small",
                "a non-all-in bet must reach at least the big blind");
        }
        return common::Status::Ok();
    }

    case ActionKind::RaiseTo: {
        if (state.current_bet <= 0) {
            return common::MakeError("raise_requires_bet",
                                     "RaiseTo requires an existing bet");
        }
        if (!legal.can_raise) {
            return common::MakeError(
                "no_raise_right",
                "player may not bet or raise in this situation");
        }

        const std::int64_t target = action.target_total;
        if (target <= state.current_bet) {
            return common::MakeError(
                "target_not_above_current_bet",
                "raise target must be greater than the current bet");
        }
        if (target > max_target) {
            return common::MakeError(
                "target_too_large",
                "raise target exceeds the player's stack");
        }

        const bool is_all_in = target == max_target;
        if (target < min_raise_to && !is_all_in) {
            return common::MakeError(
                "target_too_small",
                "raise target is below the minimum raise and is not all-in");
        }
        return common::Status::Ok();
    }

    case ActionKind::AllIn: {
        if (player.stack <= 0) {
            return common::MakeError("no_chips", "player has no chips left");
        }
        const std::int64_t target = max_target;
        if (target > state.current_bet && !legal.can_raise) {
            return common::MakeError(
                "no_raise_right",
                "all-in would raise but the player has no raise right");
        }
        return common::Status::Ok();
    }
    }

    return common::MakeError("invalid_action", "unknown action kind");
}

} // namespace poker::core
