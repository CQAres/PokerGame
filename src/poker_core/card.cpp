#include "poker_core/card.h"

#include <cctype>
#include <sstream>

namespace poker::core {
namespace {

std::optional<std::uint8_t> ParseRank(std::string_view text) {
    if (text == "10") {
        return 10;
    }
    if (text.size() != 1) {
        return std::nullopt;
    }
    return CharToRank(text.front());
}

} // namespace

bool operator==(const Card& lhs, const Card& rhs) noexcept {
    return lhs.rank == rhs.rank && lhs.suit == rhs.suit;
}

bool operator!=(const Card& lhs, const Card& rhs) noexcept {
    return !(lhs == rhs);
}

char RankToChar(std::uint8_t rank) noexcept {
    switch (rank) {
    case 10:
        return 'T';
    case 11:
        return 'J';
    case 12:
        return 'Q';
    case 13:
        return 'K';
    case 14:
        return 'A';
    default:
        return static_cast<char>('0' + rank);
    }
}

std::optional<std::uint8_t> CharToRank(char rank) noexcept {
    const char upper = static_cast<char>(std::toupper(
        static_cast<unsigned char>(rank)));
    if (upper >= '2' && upper <= '9') {
        return static_cast<std::uint8_t>(upper - '0');
    }
    switch (upper) {
    case 'T':
        return 10;
    case 'J':
        return 11;
    case 'Q':
        return 12;
    case 'K':
        return 13;
    case 'A':
        return 14;
    default:
        return std::nullopt;
    }
}

char SuitToChar(Suit suit) noexcept {
    switch (suit) {
    case Suit::Clubs:
        return 'c';
    case Suit::Diamonds:
        return 'd';
    case Suit::Hearts:
        return 'h';
    case Suit::Spades:
        return 's';
    }
    return '?';
}

std::optional<Suit> CharToSuit(char suit) noexcept {
    switch (static_cast<char>(std::tolower(
        static_cast<unsigned char>(suit)))) {
    case 'c':
        return Suit::Clubs;
    case 'd':
        return Suit::Diamonds;
    case 'h':
        return Suit::Hearts;
    case 's':
        return Suit::Spades;
    default:
        return std::nullopt;
    }
}

std::string ToString(const Card& card) {
    std::string result;
    result += RankToChar(card.rank);
    result += SuitToChar(card.suit);
    return result;
}

std::optional<Card> ParseCard(std::string_view text) {
    if (text.size() < 2) {
        return std::nullopt;
    }

    const char suitChar = text.back();
    const std::string_view rankText = text.substr(0, text.size() - 1);
    const auto rank = ParseRank(rankText);
    const auto suit = CharToSuit(suitChar);
    if (!rank.has_value() || !suit.has_value()) {
        return std::nullopt;
    }

    const Card card{*rank, *suit};
    if (!card.IsValid()) {
        return std::nullopt;
    }
    return card;
}

common::Result<std::vector<Card>> ParseCards(std::string_view text) {
    std::istringstream input{std::string(text)};
    std::vector<Card> cards;
    std::string token;

    while (input >> token) {
        const auto card = ParseCard(token);
        if (!card.has_value()) {
            return common::MakeError("invalid_card",
                                     "invalid card token: " + token);
        }
        cards.push_back(*card);
    }

    return cards;
}

int CardIndex(const Card& card) noexcept {
    if (!card.IsValid()) {
        return -1;
    }
    return (static_cast<int>(card.rank) - 2) * 4 +
           static_cast<int>(card.suit);
}

std::optional<Card> CardFromIndex(int index) {
    if (index < 0 || index >= 52) {
        return std::nullopt;
    }

    const int rank = index / 4 + 2;
    const int suit = index % 4;
    const Card card{static_cast<std::uint8_t>(rank),
                    static_cast<Suit>(suit)};
    if (!card.IsValid()) {
        return std::nullopt;
    }
    return card;
}

} // namespace poker::core
