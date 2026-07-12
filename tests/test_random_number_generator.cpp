#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

#include "petriterm/engine/RandomNumberGenerator.hpp"

using petriterm::engine::RandomNumberGenerator;

TEST_CASE("RandomNumberGenerator reproduces the same sequence for a fixed seed", "[rng]") {
    RandomNumberGenerator first(12345);
    RandomNumberGenerator second(12345);
    for (int iteration = 0; iteration < 200; ++iteration) {
        REQUIRE(first.integerInRange(0, 1000000) == second.integerInRange(0, 1000000));
    }
}

TEST_CASE("RandomNumberGenerator decorrelates distinct seeds", "[rng]") {
    RandomNumberGenerator first(1);
    RandomNumberGenerator second(2);
    bool sequencesDiffer = false;
    for (int iteration = 0; iteration < 200; ++iteration) {
        if (first.integerInRange(0, 1000000) != second.integerInRange(0, 1000000)) {
            sequencesDiffer = true;
        }
    }
    REQUIRE(sequencesDiffer);
}

TEST_CASE("integerInRange stays within the inclusive bounds", "[rng]") {
    RandomNumberGenerator rng(7);
    for (int iteration = 0; iteration < 1000; ++iteration) {
        const int value = rng.integerInRange(-5, 5);
        REQUIRE(value >= -5);
        REQUIRE(value <= 5);
    }
}

TEST_CASE("integerInRange with equal bounds returns that value", "[rng]") {
    RandomNumberGenerator rng(7);
    for (int iteration = 0; iteration < 20; ++iteration) {
        REQUIRE(rng.integerInRange(4, 4) == 4);
    }
}

TEST_CASE("unitInterval stays within the half-open unit interval", "[rng]") {
    RandomNumberGenerator rng(99);
    for (int iteration = 0; iteration < 1000; ++iteration) {
        const double value = rng.unitInterval();
        REQUIRE(value >= 0.0);
        REQUIRE(value < 1.0);
    }
}

TEST_CASE("chance is never true at probability 0 and always true at probability 1",
          "[rng]") {
    RandomNumberGenerator rng(3);
    for (int iteration = 0; iteration < 200; ++iteration) {
        REQUIRE_FALSE(rng.chance(0.0));
        REQUIRE(rng.chance(1.0));
    }
}

TEST_CASE("pickUniformly returns an element of the range", "[rng]") {
    RandomNumberGenerator rng(42);
    std::array<int, 5> values{10, 20, 30, 40, 50};
    for (int iteration = 0; iteration < 200; ++iteration) {
        const int chosen = rng.pickUniformly(values);
        bool foundInRange = false;
        for (const int candidate : values) {
            if (candidate == chosen) {
                foundInRange = true;
            }
        }
        REQUIRE(foundInRange);
    }
}

TEST_CASE("pickUniformly on a single element returns that element", "[rng]") {
    RandomNumberGenerator rng(42);
    std::vector<int> values{77};
    REQUIRE(rng.pickUniformly(values) == 77);
}
