#pragma once

#include <cstdint>

namespace petriterm::world {

/// Deterministic value-noise generator for terrain, temperature, and humidity
/// fields. Lattice values are derived by hashing integer coordinates with the
/// seed, so every query is a pure function of (seed, position): the same seed
/// always reproduces the same field, with no internal state to advance.
class NoiseGenerator {
public:
    /// Constructs a noise generator seeded for reproducibility. Distinct seeds
    /// produce decorrelated fields.
    explicit NoiseGenerator(std::uint64_t seed);

    /// Returns smoothly interpolated value noise in [0.0, 1.0) at the given
    /// continuous coordinate: pseudo-random values fixed at integer lattice
    /// points, blended between them with a smoothstep-faded bilinear mix.
    double valueNoiseAt(double sampleX, double sampleY) const;

    /// Returns fractal value noise in [0.0, 1.0] at the given continuous
    /// coordinate: octaveCount layers of value noise summed with doubling
    /// frequency and per-octave amplitude decay given by persistence, then
    /// normalized so the result stays in range. Precondition: octaveCount >= 1
    /// and persistence > 0.
    double fractalNoiseAt(double sampleX, double sampleY, int octaveCount,
                          double persistence) const;

private:
    double latticeValueAt(std::int64_t latticeX, std::int64_t latticeY) const;

    std::uint64_t seed;
};

}
