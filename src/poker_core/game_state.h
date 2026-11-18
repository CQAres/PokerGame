#pragma once

#include <cstdint>
#include <vector>

#include "poker_core/card.h"
#include "poker_core/player.h"

namespace poker::core {

enum class Street : std::uint8_t {
    Waiting = 0,
    Preflop,
    Flop,
    Turn,
    River,
    Showdown,
    Finished,
};

const char* StreetName(Street street) noexcept;
bool IsBettingStreet(Street street) noexcept;

struct GameState {
    std::uint64_t hand_id = 0;
    std::uint64_t state_version = 0;
    std::uint64_t action_seq = 0;
    std::uint64_t hand_seed = 0;

    Street street = Street::Waiting;
    std::vector<Player> players;
    std::vector<Card> board;
    std::vector<Card> burn_cards;

    int dealer_seat = -1;
    int small_blind_seat = -1;
    int big_blind_seat = -1;
    int acting_seat = -1;

    std::int64_t current_bet = 0;
    std::int64_t last_full_raise_size = 0;
    std::int64_t last_full_raise_to = 0;

    std::int64_t small_blind = 5;
    std::int64_t big_blind = 10;

    bool hand_finished = true;
    bool uncontested = false;

    [[nodiscard]] std::int64_t TotalPot() const noexcept;
    [[nodiscard]] int CountNonFolded() const noexcept;
    [[nodiscard]] int CountActionable() const noexcept;
    [[nodiscard]] int CountActive() const noexcept;
    [[nodiscard]] bool IsBettingRoundComplete() const noexcept;

    Player* PlayerAtSeat(int seat) noexcept;
    const Player* PlayerAtSeat(int seat) const noexcept;
    Player* FindPlayerById(std::uint64_t player_id) noexcept;
    const Player* FindPlayerById(std::uint64_t player_id) const noexcept;

    // Exclusive scans: start looking at from_seat + 1.
    int NextActiveSeat(int from_seat) const noexcept;
    int NextActionSeat(int from_seat) const noexcept;
};

} // namespace poker::core
