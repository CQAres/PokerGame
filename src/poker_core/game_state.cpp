#include "poker_core/game_state.h"

namespace poker::core {

const char* StreetName(Street street) noexcept {
    switch (street) {
    case Street::Waiting:
        return "waiting";
    case Street::Preflop:
        return "preflop";
    case Street::Flop:
        return "flop";
    case Street::Turn:
        return "turn";
    case Street::River:
        return "river";
    case Street::Showdown:
        return "showdown";
    case Street::Finished:
        return "finished";
    }
    return "unknown";
}

bool IsBettingStreet(Street street) noexcept {
    return street == Street::Preflop || street == Street::Flop ||
           street == Street::Turn || street == Street::River;
}

std::int64_t GameState::TotalPot() const noexcept {
    std::int64_t total = 0;
    for (const Player& player : players) {
        total += player.committed_hand;
    }
    return total;
}

int GameState::CountNonFolded() const noexcept {
    int count = 0;
    for (const Player& player : players) {
        if (player.is_active && !player.folded) {
            ++count;
        }
    }
    return count;
}

int GameState::CountActionable() const noexcept {
    int count = 0;
    for (const Player& player : players) {
        if (player.IsActionable()) {
            ++count;
        }
    }
    return count;
}

int GameState::CountActive() const noexcept {
    int count = 0;
    for (const Player& player : players) {
        if (player.is_active) {
            ++count;
        }
    }
    return count;
}

bool GameState::IsBettingRoundComplete() const noexcept {
    const int actionable = CountActionable();
    if (actionable == 0) {
        return true;
    }

    if (actionable == 1) {
        // A lone actionable player may still have chips to call, but there is
        // nobody who can respond to a raise. Once the call is matched, end
        // the round instead of requesting a meaningless check.
        for (const Player& player : players) {
            if (player.IsActionable()) {
                return player.committed_round >= current_bet;
            }
        }
        return true;
    }

    for (const Player& player : players) {
        if (player.IsActionable() && player.needs_action) {
            return false;
        }
    }
    return true;
}

Player* GameState::PlayerAtSeat(int seat) noexcept {
    if (seat < 0 || seat >= static_cast<int>(players.size())) {
        return nullptr;
    }
    return &players[static_cast<std::size_t>(seat)];
}

const Player* GameState::PlayerAtSeat(int seat) const noexcept {
    if (seat < 0 || seat >= static_cast<int>(players.size())) {
        return nullptr;
    }
    return &players[static_cast<std::size_t>(seat)];
}

Player* GameState::FindPlayerById(std::uint64_t player_id) noexcept {
    for (Player& player : players) {
        if (player.id == player_id && player.IsSeated()) {
            return &player;
        }
    }
    return nullptr;
}

const Player* GameState::FindPlayerById(std::uint64_t player_id) const noexcept {
    for (const Player& player : players) {
        if (player.id == player_id && player.IsSeated()) {
            return &player;
        }
    }
    return nullptr;
}

int GameState::NextActiveSeat(int from_seat) const noexcept {
    const int count = static_cast<int>(players.size());
    if (count == 0) {
        return -1;
    }
    for (int offset = 1; offset <= count; ++offset) {
        const int seat = (from_seat + offset) % count;
        if (players[static_cast<std::size_t>(seat)].is_active) {
            return seat;
        }
    }
    return -1;
}

int GameState::NextActionSeat(int from_seat) const noexcept {
    const int count = static_cast<int>(players.size());
    if (count == 0) {
        return -1;
    }
    for (int offset = 1; offset <= count; ++offset) {
        const int seat = (from_seat + offset) % count;
        const Player& player = players[static_cast<std::size_t>(seat)];
        if (player.IsActionable() && player.needs_action) {
            return seat;
        }
    }
    return -1;
}

} // namespace poker::core
