#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "petriterm/world/NoiseGenerator.hpp"

using petriterm::world::NoiseGenerator;

TEST_CASE("fractal noise is identical for the same seed and coordinates", "[noise]") {
    const NoiseGenerator first(424242);
    const NoiseGenerator second(424242);
    for (int sampleIndex = 0; sampleIndex < 100; ++sampleIndex) {
        const double sampleX = sampleIndex * 0.37;
        const double sampleY = sampleIndex * 1.13;
        REQUIRE(first.fractalNoiseAt(sampleX, sampleY, 4, 0.5) ==
                second.fractalNoiseAt(sampleX, sampleY, 4, 0.5));
    }
}

TEST_CASE("repeated queries of one generator return the same value", "[noise]") {
    const NoiseGenerator generator(7);
    const double firstResult = generator.fractalNoiseAt(3.25, 8.75, 5, 0.6);
    for (int repeat = 0; repeat < 10; ++repeat) {
        REQUIRE(generator.fractalNoiseAt(3.25, 8.75, 5, 0.6) == firstResult);
    }
}

TEST_CASE("different seeds produce decorrelated noise fields", "[noise]") {
    const NoiseGenerator first(1);
    const NoiseGenerator second(2);
    int differingSampleCount = 0;
    for (int sampleIndex = 0; sampleIndex < 100; ++sampleIndex) {
        const double sampleX = sampleIndex * 0.61;
        const double sampleY = sampleIndex * 0.29;
        if (first.fractalNoiseAt(sampleX, sampleY, 4, 0.5) !=
            second.fractalNoiseAt(sampleX, sampleY, 4, 0.5)) {
            ++differingSampleCount;
        }
    }
    REQUIRE(differingSampleCount > 90);
}

TEST_CASE("fractal noise stays within the unit interval", "[noise]") {
    const NoiseGenerator generator(987654321);
    for (int sampleIndex = 0; sampleIndex < 500; ++sampleIndex) {
        const double sampleX = (sampleIndex % 25) * 0.83 - 7.0;
        const double sampleY = (sampleIndex / 25) * 0.59 - 3.0;
        const double noiseValue = generator.fractalNoiseAt(sampleX, sampleY, 6, 0.5);
        REQUIRE(noiseValue >= 0.0);
        REQUIRE(noiseValue <= 1.0);
    }
}

TEST_CASE("value noise is continuous across small steps", "[noise]") {
    const NoiseGenerator generator(31337);
    const double stepSize = 0.01;
    for (int sampleIndex = 0; sampleIndex < 400; ++sampleIndex) {
        const double sampleX = sampleIndex * stepSize;
        const double difference = std::abs(generator.valueNoiseAt(sampleX, 4.5) -
                                           generator.valueNoiseAt(sampleX + stepSize, 4.5));
        REQUIRE(difference < 0.1);
    }
}

TEST_CASE("noise varies across space rather than being constant", "[noise]") {
    const NoiseGenerator generator(555);
    double minimumSeen = 1.0;
    double maximumSeen = 0.0;
    for (int sampleIndex = 0; sampleIndex < 200; ++sampleIndex) {
        const double noiseValue =
            generator.fractalNoiseAt(sampleIndex * 0.47, sampleIndex * 0.31, 4, 0.5);
        minimumSeen = std::min(minimumSeen, noiseValue);
        maximumSeen = std::max(maximumSeen, noiseValue);
    }
    REQUIRE(maximumSeen - minimumSeen > 0.3);
}
