#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "poker_core/card.h"
#include "poker_core/rng.h"

namespace poker::core {

class Deck {
public:
    static constexpr std::size_t kCardCount = 52;

    Deck();

    // Restores the full 52-card deck in canonical order and rewinds.
    void Reset();

    // Fisher-Yates shuffle over the full deck, then rewinds to the top.
    // The implementation is deterministic for a given Rng seed.
    void Shuffle(Rng& rng);

    std::optional<Card> Draw();

    [[nodiscard]] std::size_t Remaining() const noexcept {
        return cards_.size() - next_;
    }

    [[nodiscard]] bool Empty() const noexcept {
        return next_ >= cards_.size();
    }

    [[nodiscard]] const std::vector<Card>& Cards() const noexcept {
        return cards_;
    }

    [[nodiscard]] std::size_t NextIndex() const noexcept {
        return next_;
    }

private:
    std::vector<Card> cards_;
    std::size_t next_ = 0;
};

} // namespace poker::core
