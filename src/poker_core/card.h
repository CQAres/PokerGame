#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "common/result.h"

namespace poker::core {

enum class Suit : std::uint8_t {
    Clubs = 0,
    Diamonds = 1,
    Hearts = 2,
    Spades = 3,
};

inline constexpr std::uint8_t kMinRank = 2;
inline constexpr std::uint8_t kMaxRank = 14;

struct Card {
    std::uint8_t rank = 0; // 2..14, A = 14
    Suit suit = Suit::Clubs;

    [[nodiscard]] bool IsValid() const noexcept {
        return rank >= kMinRank && rank <= kMaxRank &&
               static_cast<std::uint8_t>(suit) <= 3;
    }
};

bool operator==(const Card& lhs, const Card& rhs) noexcept;
bool operator!=(const Card& lhs, const Card& rhs) noexcept;

char RankToChar(std::uint8_t rank) noexcept;
std::optional<std::uint8_t> CharToRank(char rank) noexcept;
char SuitToChar(Suit suit) noexcept;
std::optional<Suit> CharToSuit(char suit) noexcept;

// Formats a card as rank + suit, e.g. "As", "Td", "9h".
std::string ToString(const Card& card);

// Accepts "As", "td", "10h", etc. Case-insensitive.
std::optional<Card> ParseCard(std::string_view text);

// Parses whitespace-separated cards. Returns an error for the first bad token.
common::Result<std::vector<Card>> ParseCards(std::string_view text);

// Canonical card index in [0, 51], or -1 for an invalid card.
int CardIndex(const Card& card) noexcept;
std::optional<Card> CardFromIndex(int index);

} // namespace poker::core
