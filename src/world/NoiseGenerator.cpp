#include "petriterm/world/NoiseGenerator.hpp"

#include <cassert>
#include <cmath>

namespace petriterm::world {

namespace {

/// Scrambles a 64-bit value into an avalanche-quality hash (finalizer from
/// MurmurHash3), so nearby lattice coordinates yield unrelated values.
std::uint64_t mixBits(std::uint64_t value) {
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33;
    return value;
}

/// Maps a hash to a double in [0.0, 1.0) using its top 53 bits, matching the
/// mantissa width of a double so the mapping is uniform.
double hashToUnitInterval(std::uint64_t hash) {
    return static_cast<double>(hash >> 11) * 0x1.0p-53;
}

/// Smoothstep fade curve: eases the interpolation weight so the noise gradient
/// is continuous across lattice-cell boundaries instead of visibly creased.
double smoothstepFade(double linearWeight) {
    return linearWeight * linearWeight * (3.0 - 2.0 * linearWeight);
}

double interpolateLinearly(double start, double end, double weight) {
    return start + (end - start) * weight;
}

}

NoiseGenerator::NoiseGenerator(std::uint64_t seed) : seed(seed) {}

double NoiseGenerator::latticeValueAt(std::int64_t latticeX, std::int64_t latticeY) const {
    const std::uint64_t coordinateHash =
        mixBits(mixBits(seed ^ static_cast<std::uint64_t>(latticeX)) ^
                static_cast<std::uint64_t>(latticeY));
    return hashToUnitInterval(coordinateHash);
}

double NoiseGenerator::valueNoiseAt(double sampleX, double sampleY) const {
    const auto cellX = static_cast<std::int64_t>(std::floor(sampleX));
    const auto cellY = static_cast<std::int64_t>(std::floor(sampleY));
    const double weightX = smoothstepFade(sampleX - static_cast<double>(cellX));
    const double weightY = smoothstepFade(sampleY - static_cast<double>(cellY));

    const double topEdge = interpolateLinearly(latticeValueAt(cellX, cellY),
                                               latticeValueAt(cellX + 1, cellY), weightX);
    const double bottomEdge = interpolateLinearly(
        latticeValueAt(cellX, cellY + 1), latticeValueAt(cellX + 1, cellY + 1), weightX);
    return interpolateLinearly(topEdge, bottomEdge, weightY);
}

double NoiseGenerator::fractalNoiseAt(double sampleX, double sampleY, int octaveCount,
                                      double persistence) const {
    assert(octaveCount >= 1);
    assert(persistence > 0.0);
    double weightedNoiseSum = 0.0;
    double amplitudeSum = 0.0;
    double amplitude = 1.0;
    double frequency = 1.0;
    for (int octaveIndex = 0; octaveIndex < octaveCount; ++octaveIndex) {
        weightedNoiseSum +=
            amplitude * valueNoiseAt(sampleX * frequency, sampleY * frequency);
        amplitudeSum += amplitude;
        amplitude *= persistence;
        frequency *= 2.0;
    }
    return weightedNoiseSum / amplitudeSum;
}

}
