#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "common/result.h"
#include "poker_core/action.h"
#include "poker_core/action_validator.h"
#include "poker_core/deck.h"
#include "poker_core/game_state.h"
#include "poker_core/rng.h"
#include "poker_core/table_config.h"

namespace poker::core {

struct GameEvent {
    std::string type;
    int seat = -1;
    Street street = Street::Waiting;
    std::int64_t amount = 0;
    std::int64_t target_total = 0;
    std::string detail;
};

struct ApplyResult {
    bool hand_finished = false;
    bool street_changed = false;
    bool showdown = false;
    bool uncontested = false;
    std::vector<GameEvent> events;
};

class GameEngine {
public:
    // Throws std::invalid_argument when the config fails validation.
    explicit GameEngine(TableConfig config);

    // Seating changes are only allowed between hands.
    common::Status SeatPlayer(int seat,
                              std::uint64_t player_id,
                              std::string name,
                              PlayerType type,
                              std::int64_t stack = -1);
    common::Status RemovePlayer(std::uint64_t player_id);
    common::Status SetSittingOut(std::uint64_t player_id, bool sitting_out);

    common::Status StartHand();
    common::Result<LegalActions> GetLegalActions(
        std::uint64_t player_id) const;
    common::Result<ActionCandidate> FindCandidate(
        std::uint64_t player_id, const std::string& action_id) const;
    common::Result<ApplyResult> ApplyAction(std::uint64_t player_id,
                                            const Action& action);

    [[nodiscard]] const GameState& State() const noexcept { return state_; }
    [[nodiscard]] const TableConfig& Config() const noexcept { return config_; }
    [[nodiscard]] bool IsHandFinished() const noexcept {
        return state_.hand_finished;
    }
    [[nodiscard]] bool IsBettingRoundComplete() const noexcept {
        return state_.IsBettingRoundComplete();
    }
    [[nodiscard]] int CountNonFolded() const noexcept {
        return state_.CountNonFolded();
    }
    [[nodiscard]] int CountActionable() const noexcept {
        return state_.CountActionable();
    }
    int NextActionSeat(int from_seat) const noexcept {
        return state_.NextActionSeat(from_seat);
    }

private:
    common::Status EnsureBetweenHands() const;
    void MarkActivePlayers();
    void ResetPlayersForNewHand();
    void MoveButtonForNextHand(bool first_hand);
    void PostBlinds();
    common::Status DealHoleCards();
    common::Status DealStreetCards(Street street);
    void ResetRoundState();
    common::Status AdvanceStreet();
    common::Status EndBettingRound();
    common::Status RunoutAndShowdown();
    common::Status ResolveShowdown();
    common::Status ResolveUncontested();
    common::Status AdvanceToNextActor(int from_seat);
    void RefreshPlayersAfterRaise(int raiser_seat, bool full_raise);

    TableConfig config_;
    GameState state_;
    Deck deck_;
    Rng session_rng_;
};

} // namespace poker::core
