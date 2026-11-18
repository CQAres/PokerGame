#include "poker_core/game_engine.h"

#include <algorithm>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace poker::core {
namespace {

Street NextStreet(Street street) noexcept {
    switch (street) {
    case Street::Preflop:
        return Street::Flop;
    case Street::Flop:
        return Street::Turn;
    case Street::Turn:
        return Street::River;
    case Street::River:
        return Street::Showdown;
    default:
        return Street::Showdown;
    }
}

} // namespace

GameEngine::GameEngine(TableConfig config) : config_(std::move(config)) {
    const common::Status status = ValidateTableConfig(config_);
    if (!status.ok()) {
        throw std::invalid_argument(status.error().code + ": " +
                                    status.error().message);
    }

    state_.players.resize(static_cast<std::size_t>(config_.seat_count));
    for (int seat = 0; seat < config_.seat_count; ++seat) {
        state_.players[static_cast<std::size_t>(seat)].seat = seat;
    }
    state_.small_blind = config_.small_blind;
    state_.big_blind = config_.big_blind;
    state_.hand_finished = true;

    std::uint64_t seed = config_.seed;
    if (seed == 0) {
        std::random_device device;
        seed = (static_cast<std::uint64_t>(device()) << 32) ^
               static_cast<std::uint64_t>(device());
    }
    session_rng_.Seed(seed);
}

common::Status GameEngine::EnsureBetweenHands() const {
    if (!state_.hand_finished) {
        return common::MakeError("hand_in_progress",
                                 "operation is only allowed between hands");
    }
    return common::Status::Ok();
}

common::Status GameEngine::SeatPlayer(int seat,
                                      std::uint64_t player_id,
                                      std::string name,
                                      PlayerType type,
                                      std::int64_t stack) {
    const common::Status between = EnsureBetweenHands();
    if (!between.ok()) {
        return between;
    }
    if (seat < 0 || seat >= config_.seat_count) {
        return common::MakeError("invalid_seat", "seat is outside the table");
    }
    if (player_id == kEmptyPlayerId) {
        return common::MakeError("invalid_player_id",
                                 "player id zero is reserved");
    }
    if (state_.FindPlayerById(player_id) != nullptr) {
        return common::MakeError("duplicate_player_id",
                                 "player id is already seated");
    }

    Player& player = state_.players[static_cast<std::size_t>(seat)];
    if (player.IsSeated()) {
        return common::MakeError("seat_occupied", "seat is already occupied");
    }

    if (stack == -1) {
        stack = config_.starting_stack;
    }
    if (stack < 0) {
        return common::MakeError("invalid_stack",
                                 "stack must not be negative");
    }

    player = Player{};
    player.id = player_id;
    player.seat = seat;
    player.name = std::move(name);
    player.type = type;
    player.stack = stack;
    return common::Status::Ok();
}

common::Status GameEngine::RemovePlayer(std::uint64_t player_id) {
    const common::Status between = EnsureBetweenHands();
    if (!between.ok()) {
        return between;
    }
    Player* player = state_.FindPlayerById(player_id);
    if (player == nullptr) {
        return common::MakeError("player_not_found", "player is not seated");
    }

    const int seat = player->seat;
    *player = Player{};
    player->seat = seat;
    return common::Status::Ok();
}

common::Status GameEngine::SetSittingOut(std::uint64_t player_id,
                                         bool sitting_out) {
    const common::Status between = EnsureBetweenHands();
    if (!between.ok()) {
        return between;
    }
    Player* player = state_.FindPlayerById(player_id);
    if (player == nullptr) {
        return common::MakeError("player_not_found", "player is not seated");
    }
    player->sitting_out = sitting_out;
    return common::Status::Ok();
}

void GameEngine::MarkActivePlayers() {
    for (Player& player : state_.players) {
        player.is_active = player.IsSeated() && !player.sitting_out &&
                           player.stack > 0;
    }
}

void GameEngine::ResetPlayersForNewHand() {
    for (Player& player : state_.players) {
        player.committed_hand = 0;
        player.committed_round = 0;
        player.folded = false;
        player.all_in = false;
        player.needs_action = false;
        player.acted_since_full_raise = false;
        player.cards_visible = false;
        player.hole_cards.clear();
        player.last_action.clear();
    }
}

void GameEngine::MoveButtonForNextHand(bool first_hand) {
    const int seat_count = config_.seat_count;
    int start = first_hand
                    ? config_.first_button_seat
                    : (state_.dealer_seat + 1 + seat_count) % seat_count;

    for (int offset = 0; offset < seat_count; ++offset) {
        const int seat = (start + offset) % seat_count;
        if (state_.players[static_cast<std::size_t>(seat)].is_active) {
            state_.dealer_seat = seat;
            return;
        }
    }
    state_.dealer_seat = -1;
}

void GameEngine::PostBlinds() {
    int small_blind_seat = -1;
    int big_blind_seat = -1;

    if (state_.CountActive() == 2) {
        small_blind_seat = state_.dealer_seat;
        big_blind_seat = state_.NextActiveSeat(small_blind_seat);
    } else {
        small_blind_seat = state_.NextActiveSeat(state_.dealer_seat);
        big_blind_seat = state_.NextActiveSeat(small_blind_seat);
    }

    state_.small_blind_seat = small_blind_seat;
    state_.big_blind_seat = big_blind_seat;

    const auto post_blind = [this](int seat, std::int64_t blind) {
        Player& player = state_.players[static_cast<std::size_t>(seat)];
        const std::int64_t amount = std::min(player.stack, blind);
        player.stack -= amount;
        player.committed_hand += amount;
        player.committed_round += amount;
        if (player.stack == 0) {
            player.all_in = true;
        }
    };

    post_blind(small_blind_seat, config_.small_blind);
    post_blind(big_blind_seat, config_.big_blind);

    // A short big blind does not reduce the amount other players must call.
    state_.current_bet = config_.big_blind;
    state_.last_full_raise_size = config_.big_blind;
    state_.last_full_raise_to = config_.big_blind;
}

common::Status GameEngine::DealHoleCards() {
    deck_.Reset();
    Rng hand_rng(state_.hand_seed);
    deck_.Shuffle(hand_rng);

    const int active_count = state_.CountActive();
    std::vector<int> order;
    order.reserve(static_cast<std::size_t>(active_count));

    int seat = state_.NextActiveSeat(state_.dealer_seat);
    for (int i = 0; i < active_count; ++i) {
        if (seat < 0) {
            return common::MakeError("invalid_state",
                                     "could not build dealing order");
        }
        order.push_back(seat);
        seat = state_.NextActiveSeat(seat);
    }

    for (int round = 0; round < 2; ++round) {
        for (const int deal_seat : order) {
            const auto card = deck_.Draw();
            if (!card.has_value()) {
                return common::MakeError("deck_exhausted",
                                         "not enough cards to deal");
            }
            state_.players[static_cast<std::size_t>(deal_seat)]
                .hole_cards.push_back(*card);
        }
    }
    return common::Status::Ok();
}

common::Status GameEngine::StartHand() {
    if (!state_.hand_finished) {
        return common::MakeError("hand_in_progress",
                                 "the current hand has not finished");
    }

    MarkActivePlayers();
    if (state_.CountActive() < 2) {
        return common::MakeError("not_enough_players",
                                 "at least two active players are required");
    }

    const bool first_hand = state_.hand_id == 0;
    ++state_.hand_id;
    state_.hand_seed = session_rng_.Next();
    state_.action_seq = 0;
    state_.board.clear();
    state_.burn_cards.clear();
    state_.street = Street::Preflop;
    state_.hand_finished = false;
    state_.uncontested = false;
    state_.acting_seat = -1;
    state_.current_bet = 0;
    state_.last_full_raise_size = config_.big_blind;
    state_.last_full_raise_to = 0;

    ResetPlayersForNewHand();
    MoveButtonForNextHand(first_hand);
    PostBlinds();

    const common::Status dealt = DealHoleCards();
    if (!dealt.ok()) {
        state_.hand_finished = true;
        state_.street = Street::Waiting;
        return dealt;
    }

    for (Player& player : state_.players) {
        if (player.IsActionable()) {
            player.needs_action = true;
        }
    }

    ++state_.state_version;

    if (state_.IsBettingRoundComplete()) {
        return EndBettingRound();
    }

    state_.acting_seat = state_.NextActionSeat(state_.big_blind_seat);
    if (state_.acting_seat < 0) {
        return EndBettingRound();
    }
    return common::Status::Ok();
}

common::Status GameEngine::DealStreetCards(Street street) {
    if (street != Street::Flop && street != Street::Turn &&
        street != Street::River) {
        return common::MakeError("invalid_state",
                                 "no cards are dealt on this street");
    }

    const auto burn = deck_.Draw();
    if (!burn.has_value()) {
        return common::MakeError("deck_exhausted", "not enough cards to burn");
    }
    state_.burn_cards.push_back(*burn);

    const int count = street == Street::Flop ? 3 : 1;
    for (int i = 0; i < count; ++i) {
        const auto card = deck_.Draw();
        if (!card.has_value()) {
            return common::MakeError("deck_exhausted",
                                     "not enough cards for the board");
        }
        state_.board.push_back(*card);
    }
    return common::Status::Ok();
}

void GameEngine::ResetRoundState() {
    state_.current_bet = 0;
    state_.last_full_raise_size = config_.big_blind;
    state_.last_full_raise_to = 0;

    for (Player& player : state_.players) {
        player.committed_round = 0;
        player.acted_since_full_raise = false;
        player.needs_action = player.IsActionable();
    }
}

common::Status GameEngine::AdvanceStreet() {
    Street next = Street::Showdown;
    switch (state_.street) {
    case Street::Preflop:
        next = Street::Flop;
        break;
    case Street::Flop:
        next = Street::Turn;
        break;
    case Street::Turn:
        next = Street::River;
        break;
    case Street::River:
        return ResolveShowdown();
    default:
        return common::MakeError("invalid_state",
                                 "cannot advance from the current street");
    }

    const common::Status dealt = DealStreetCards(next);
    if (!dealt.ok()) {
        return dealt;
    }

    state_.street = next;
    ResetRoundState();
    ++state_.state_version;

    state_.acting_seat = state_.NextActionSeat(state_.dealer_seat);
    if (state_.acting_seat < 0) {
        return EndBettingRound();
    }
    return common::Status::Ok();
}

common::Status GameEngine::EndBettingRound() {
    state_.acting_seat = -1;
    for (Player& player : state_.players) {
        player.needs_action = false;
    }

    const int non_folded = state_.CountNonFolded();
    if (non_folded == 0) {
        return common::MakeError("invalid_state",
                                 "no non-folded players remain");
    }
    if (non_folded == 1) {
        return ResolveUncontested();
    }
    if (state_.CountActionable() <= 1) {
        return RunoutAndShowdown();
    }
    return AdvanceStreet();
}

common::Status GameEngine::RunoutAndShowdown() {
    // No more decisions are possible, so hole cards can be revealed early.
    for (Player& player : state_.players) {
        if (player.is_active && !player.folded) {
            player.cards_visible = true;
        }
    }

    while (IsBettingStreet(state_.street)) {
        const Street next = NextStreet(state_.street);
        if (next == Street::Showdown) {
            break;
        }
        const common::Status dealt = DealStreetCards(next);
        if (!dealt.ok()) {
            return dealt;
        }
        state_.street = next;
        ResetRoundState();
        for (Player& player : state_.players) {
            player.needs_action = false;
        }
        ++state_.state_version;
    }

    return ResolveShowdown();
}

common::Status GameEngine::ResolveShowdown() {
    if (state_.CountNonFolded() < 1) {
        return common::MakeError("invalid_state",
                                 "cannot enter showdown without players");
    }

    state_.street = Street::Showdown;
    state_.hand_finished = true;
    state_.uncontested = false;
    state_.acting_seat = -1;
    for (Player& player : state_.players) {
        player.needs_action = false;
        if (player.is_active && !player.folded) {
            player.cards_visible = true;
        }
    }
    ++state_.state_version;
    return common::Status::Ok();
}

common::Status GameEngine::ResolveUncontested() {
    if (state_.CountNonFolded() != 1) {
        return common::MakeError("invalid_state",
                                 "uncontested hand needs exactly one player");
    }

    state_.street = Street::Finished;
    state_.hand_finished = true;
    state_.uncontested = true;
    state_.acting_seat = -1;
    for (Player& player : state_.players) {
        player.needs_action = false;
    }
    ++state_.state_version;
    return common::Status::Ok();
}

void GameEngine::RefreshPlayersAfterRaise(int raiser_seat, bool full_raise) {
    for (Player& player : state_.players) {
        if (player.seat == raiser_seat || !player.IsActionable()) {
            continue;
        }
        player.needs_action = true;
        if (full_raise) {
            player.acted_since_full_raise = false;
        }
    }
}

common::Status GameEngine::AdvanceToNextActor(int from_seat) {
    if (state_.IsBettingRoundComplete()) {
        return EndBettingRound();
    }

    const int next = state_.NextActionSeat(from_seat);
    if (next < 0) {
        return EndBettingRound();
    }
    state_.acting_seat = next;
    return common::Status::Ok();
}

common::Result<LegalActions> GameEngine::GetLegalActions(
    std::uint64_t player_id) const {
    if (state_.hand_finished || !IsBettingStreet(state_.street)) {
        return common::MakeError("invalid_state",
                                 "no betting action is possible now");
    }

    const Player* player = state_.FindPlayerById(player_id);
    if (player == nullptr) {
        return common::MakeError("player_not_found", "player is not seated");
    }
    if (state_.acting_seat != player->seat) {
        return common::MakeError("not_your_turn",
                                 "player is not the acting player");
    }
    if (!player->IsActionable()) {
        return common::MakeError("player_not_actionable",
                                 "player cannot act in the current hand");
    }

    return BuildLegalActions(state_, *player);
}

common::Result<ActionCandidate> GameEngine::FindCandidate(
    std::uint64_t player_id, const std::string& action_id) const {
    const common::Result<LegalActions> legal = GetLegalActions(player_id);
    if (!legal.ok()) {
        return legal.error();
    }

    const ActionCandidate* candidate = legal.value().Find(action_id);
    if (candidate == nullptr) {
        return common::MakeError("candidate_not_found",
                                 "no legal candidate with id " + action_id);
    }
    return *candidate;
}

common::Result<ApplyResult> GameEngine::ApplyAction(
    std::uint64_t player_id, const Action& action) {
    if (state_.hand_finished || !IsBettingStreet(state_.street)) {
        return common::MakeError("invalid_state",
                                 "no betting action is possible now");
    }

    Player* player = state_.FindPlayerById(player_id);
    if (player == nullptr) {
        return common::MakeError("player_not_found", "player is not seated");
    }
    if (state_.acting_seat != player->seat) {
        return common::MakeError("not_your_turn",
                                 "player is not the acting player");
    }

    const LegalActions legal = BuildLegalActions(state_, *player);
    const common::Status valid =
        ActionValidator::Validate(state_, *player, legal, action);
    if (!valid.ok()) {
        return valid.error();
    }

    const Street street_before = state_.street;
    const int actor_seat = player->seat;
    std::int64_t paid = 0;
    std::int64_t target = player->committed_round;

    switch (action.kind) {
    case ActionKind::Fold:
        break;
    case ActionKind::Check:
        break;
    case ActionKind::Call: {
        const std::int64_t to_call = ToCall(state_, *player);
        paid = std::min(to_call, player->stack);
        target = player->committed_round + paid;
        break;
    }
    case ActionKind::BetTo:
    case ActionKind::RaiseTo:
        target = action.target_total;
        paid = target - player->committed_round;
        break;
    case ActionKind::AllIn:
        paid = player->stack;
        target = player->committed_round + paid;
        break;
    }

    if (paid > 0) {
        player->stack -= paid;
        player->committed_round += paid;
        player->committed_hand += paid;
    }

    if (action.kind == ActionKind::Fold) {
        player->folded = true;
    } else if (player->stack == 0) {
        player->all_in = true;
    }

    if (target > state_.current_bet) {
        const std::int64_t old_bet = state_.current_bet;
        const std::int64_t new_bet = target;
        state_.current_bet = new_bet;

        const std::int64_t raise_size = state_.last_full_raise_size > 0
                                            ? state_.last_full_raise_size
                                            : state_.big_blind;
        const bool completion = old_bet > 0 &&
                                old_bet < state_.big_blind &&
                                state_.last_full_raise_to == 0 &&
                                new_bet >= state_.big_blind;
        const bool full_raise =
            completion ||
            new_bet >= state_.last_full_raise_to + raise_size;

        if (full_raise) {
            std::int64_t new_raise_size = new_bet - old_bet;
            if (completion) {
                new_raise_size = state_.big_blind;
            } else if (old_bet != state_.last_full_raise_to) {
                // Several short all-ins accumulated into a full raise.
                new_raise_size = new_bet - state_.last_full_raise_to;
            }
            state_.last_full_raise_size = new_raise_size;
            state_.last_full_raise_to = new_bet;
            RefreshPlayersAfterRaise(actor_seat, true);
        } else {
            // A short all-in requires a call but does not reopen raising.
            RefreshPlayersAfterRaise(actor_seat, false);
        }
    }

    player->needs_action = false;
    player->acted_since_full_raise = true;

    std::string description = ActionKindName(action.kind);
    if (action.kind == ActionKind::Call ||
        action.kind == ActionKind::BetTo ||
        action.kind == ActionKind::RaiseTo ||
        action.kind == ActionKind::AllIn) {
        description += " " + std::to_string(paid);
    }
    if (action.kind == ActionKind::BetTo ||
        action.kind == ActionKind::RaiseTo) {
        description += " to " + std::to_string(target);
    }
    player->last_action = description;

    ++state_.action_seq;
    ++state_.state_version;

    ApplyResult result;
    result.events.push_back(GameEvent{"action", actor_seat, street_before,
                                      paid, target, description});

    common::Status advanced;
    if (state_.CountNonFolded() <= 1) {
        advanced = ResolveUncontested();
    } else {
        advanced = AdvanceToNextActor(actor_seat);
    }
    if (!advanced.ok()) {
        return advanced.error();
    }

    result.hand_finished = state_.hand_finished;
    result.street_changed = state_.street != street_before;
    result.showdown = state_.street == Street::Showdown;
    result.uncontested = state_.uncontested;

    if (result.street_changed) {
        result.events.push_back(GameEvent{"street", -1, state_.street, 0, 0,
                                          StreetName(state_.street)});
    }
    if (result.hand_finished) {
        result.events.push_back(
            GameEvent{result.showdown ? "showdown" : "hand_finished", -1,
                      state_.street, 0, 0, StreetName(state_.street)});
    }

    return result;
}

} // namespace poker::core
