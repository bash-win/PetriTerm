#pragma once

#include <cassert>
#include <cstdint>
#include <iterator>
#include <random>

namespace petriterm::engine {

/// Seedable deterministic random source wrapping std::mt19937_64. A single
/// instance is owned by the simulation and threaded by reference through every
/// component that needs randomness, so a fixed seed reproduces an entire run.
class RandomNumberGenerator {
public:
    /// Constructs a generator seeded with the given value for reproducible runs.
    explicit RandomNumberGenerator(std::uint64_t seed);

    /// Returns an integer uniformly distributed in
    /// [minimumInclusive, maximumInclusive]. Precondition: minimum <= maximum.
    int integerInRange(int minimumInclusive, int maximumInclusive);

    /// Returns a double uniformly distributed in [0.0, 1.0).
    double unitInterval();

    /// Returns true with the given probability; clamps naturally so a
    /// probability of 0.0 is never true and 1.0 is always true.
    bool chance(double probability);

    /// Returns a reference to a uniformly chosen element of the range.
    /// Precondition: the range is non-empty.
    template <typename RandomAccessRange>
    auto& pickUniformly(RandomAccessRange& range) {
        assert(std::size(range) > 0);
        const int lastIndex = static_cast<int>(std::size(range)) - 1;
        const int chosenIndex = integerInRange(0, lastIndex);
        return *(std::begin(range) + chosenIndex);
    }

private:
    std::mt19937_64 engine;
};

}
