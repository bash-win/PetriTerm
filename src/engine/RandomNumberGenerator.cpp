#include "petriterm/engine/RandomNumberGenerator.hpp"

namespace petriterm::engine {

RandomNumberGenerator::RandomNumberGenerator(std::uint64_t seed) : engine(seed) {}

int RandomNumberGenerator::integerInRange(int minimumInclusive, int maximumInclusive) {
    assert(minimumInclusive <= maximumInclusive);
    std::uniform_int_distribution<int> distribution(minimumInclusive, maximumInclusive);
    return distribution(engine);
}

double RandomNumberGenerator::unitInterval() {
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(engine);
}

bool RandomNumberGenerator::chance(double probability) {
    return unitInterval() < probability;
}

}
