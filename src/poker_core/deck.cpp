#include "poker_core/deck.h"

#include <utility>

namespace poker::core {

Deck::Deck() {
    Reset();
}

void Deck::Reset() {
    cards_.clear();
    cards_.reserve(kCardCount);

    for (int suit = 0; suit < 4; ++suit) {
        for (int rank = kMinRank; rank <= kMaxRank; ++rank) {
            cards_.push_back(Card{static_cast<std::uint8_t>(rank),
                                  static_cast<Suit>(suit)});
        }
    }
    next_ = 0;
}

void Deck::Shuffle(Rng& rng) {
    for (std::size_t i = cards_.size(); i > 1; --i) {
        const std::size_t j = rng.UniformIndex(i);
        std::swap(cards_[i - 1], cards_[j]);
    }
    next_ = 0;
}

std::optional<Card> Deck::Draw() {
    if (next_ >= cards_.size()) {
        return std::nullopt;
    }
    return cards_[next_++];
}

} // namespace poker::core
