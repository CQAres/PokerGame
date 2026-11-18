#include "poker_core/rng.h"

#include <limits>
#include <stdexcept>

namespace poker::core {

Rng::Rng(std::uint64_t seed) : engine_(seed) {}

void Rng::Seed(std::uint64_t seed) {
    engine_.seed(seed);
}

std::uint64_t Rng::Next() noexcept {
    return engine_();
}

std::uint64_t Rng::operator()() noexcept {
    return Next();
}

std::uint64_t Rng::UniformUInt64(std::uint64_t bound) {
    if (bound == 0) {
        throw std::invalid_argument("Rng::UniformUInt64 bound must be positive");
    }

    // Values below this threshold would make the modulo slightly biased.
    const std::uint64_t threshold =
        (std::numeric_limits<std::uint64_t>::max() - bound + 1) % bound;

    std::uint64_t value = 0;
    do {
        value = engine_();
    } while (value < threshold);

    return value % bound;
}

int Rng::UniformInt(int min, int max) {
    if (min > max) {
        throw std::invalid_argument("Rng::UniformInt min must be <= max");
    }

    const std::uint64_t range =
        static_cast<std::uint64_t>(static_cast<std::int64_t>(max) -
                                   static_cast<std::int64_t>(min)) +
        1;
    return min + static_cast<int>(UniformUInt64(range));
}

std::size_t Rng::UniformIndex(std::size_t bound) {
    return static_cast<std::size_t>(
        UniformUInt64(static_cast<std::uint64_t>(bound)));
}

} // namespace poker::core
