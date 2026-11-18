#pragma once

#include <cstddef>
#include <cstdint>
#include <random>

namespace poker::core {

// Deterministic, cross-platform RNG wrapper.
//
// The raw sequence comes from std::mt19937_64, which is standardized.
// UniformInt/UniformIndex use rejection sampling so deck shuffles do not
// depend on the standard library's distribution implementation.
class Rng {
public:
    using result_type = std::uint64_t;

    explicit Rng(std::uint64_t seed = 0);

    void Seed(std::uint64_t seed);

    std::uint64_t Next() noexcept;

    std::uint64_t operator()() noexcept;

    // Returns a value in [0, bound). Throws std::invalid_argument for bound == 0.
    std::uint64_t UniformUInt64(std::uint64_t bound);

    // Returns a value in [min, max], inclusive.
    int UniformInt(int min, int max);

    // Returns a value in [0, bound). Throws std::invalid_argument for bound == 0.
    std::size_t UniformIndex(std::size_t bound);

private:
    std::mt19937_64 engine_;
};

} // namespace poker::core
