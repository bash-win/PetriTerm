#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <set>
#include <vector>

#include "petriterm/engine/RandomNumberGenerator.hpp"
#include "petriterm/world/ClimateSystem.hpp"
#include "petriterm/world/WorldGrid.hpp"

using petriterm::engine::RandomNumberGenerator;
using petriterm::world::ClimateSystem;
using petriterm::world::Tile;
using petriterm::world::WeatherPattern;
using petriterm::world::WorldGrid;

namespace {

std::vector<WeatherPattern> weatherSequence(std::uint64_t seed, int tickCount) {
    RandomNumberGenerator rng(seed);
    ClimateSystem climate(rng);
    WorldGrid world(8, 8);
    std::vector<WeatherPattern> sequence;
    for (int tick = 0; tick < tickCount; ++tick) {
        climate.advanceWeatherAndApplyToWorld(world);
        sequence.push_back(climate.currentWeatherPattern());
    }
    return sequence;
}

}

TEST_CASE("the same seed reproduces the same weather sequence", "[climate]") {
    REQUIRE(weatherSequence(777, 600) == weatherSequence(777, 600));
}

TEST_CASE("different seeds produce different weather sequences", "[climate]") {
    REQUIRE(weatherSequence(1, 600) != weatherSequence(2, 600));
}

TEST_CASE("weather varies over a long run rather than staying fixed", "[climate]") {
    RandomNumberGenerator rng(42);
    ClimateSystem climate(rng);
    WorldGrid world(4, 4);
    std::set<WeatherPattern> observedPatterns;
    for (int tick = 0; tick < 3000; ++tick) {
        climate.advanceWeatherAndApplyToWorld(world);
        observedPatterns.insert(climate.currentWeatherPattern());
    }
    REQUIRE(observedPatterns.size() >= 2);
}

TEST_CASE("humidity stays within [0, 100] under any weather", "[climate]") {
    RandomNumberGenerator rng(5);
    ClimateSystem climate(rng);
    WorldGrid world(3, 3);
    world.forEachTile([](int columnIndex, int, Tile& tile) {
        tile.baseHumidityPercent = columnIndex == 0 ? 97.0 : 3.0;
        tile.baseTemperatureCelsius = 20.0;
    });
    for (int tick = 0; tick < 1000; ++tick) {
        climate.advanceWeatherAndApplyToWorld(world);
        world.forEachTile([](int, int, const Tile& tile) {
            REQUIRE(tile.currentHumidityPercent >= 0.0);
            REQUIRE(tile.currentHumidityPercent <= 100.0);
        });
    }
}

TEST_CASE("the seasonal cycle shifts temperature away from its baseline", "[climate]") {
    RandomNumberGenerator rng(9);
    ClimateSystem climate(rng);
    WorldGrid world(1, 1);
    world.tileAt(0, 0).baseTemperatureCelsius = 20.0;
    for (int tick = 0; tick < 500; ++tick) {
        climate.advanceWeatherAndApplyToWorld(world);
    }
    REQUIRE(std::abs(world.tileAt(0, 0).currentTemperatureCelsius - 20.0) > 0.5);
}
