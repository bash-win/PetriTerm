#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <set>

#include "petriterm/world/WorldGenerator.hpp"

using petriterm::world::BiomeType;
using petriterm::world::classifyBiomeFromClimate;
using petriterm::world::generateWorld;
using petriterm::world::Tile;
using petriterm::world::WorldGrid;

TEST_CASE("generateWorld reproduces an identical world for the same seed", "[worldgen]") {
    const WorldGrid firstWorld = generateWorld(80, 30, 424242);
    const WorldGrid secondWorld = generateWorld(80, 30, 424242);
    firstWorld.forEachTile([&](int columnIndex, int rowIndex, const Tile& firstTile) {
        const Tile& secondTile = secondWorld.tileAt(columnIndex, rowIndex);
        REQUIRE(firstTile.biome == secondTile.biome);
        REQUIRE(firstTile.elevationNormalized == secondTile.elevationNormalized);
        REQUIRE(firstTile.baseTemperatureCelsius == secondTile.baseTemperatureCelsius);
        REQUIRE(firstTile.baseHumidityPercent == secondTile.baseHumidityPercent);
    });
}

TEST_CASE("generateWorld produces different worlds for different seeds", "[worldgen]") {
    const WorldGrid firstWorld = generateWorld(80, 30, 1);
    const WorldGrid secondWorld = generateWorld(80, 30, 2);
    int differingBiomeCount = 0;
    firstWorld.forEachTile([&](int columnIndex, int rowIndex, const Tile& firstTile) {
        if (firstTile.biome != secondWorld.tileAt(columnIndex, rowIndex).biome) {
            ++differingBiomeCount;
        }
    });
    REQUIRE(differingBiomeCount > 0);
}

TEST_CASE("default-sized worlds contain all five biomes", "[worldgen]") {
    for (const std::uint64_t seed : {1ULL, 42ULL, 12345ULL}) {
        const WorldGrid world =
            generateWorld(petriterm::world::kDefaultWorldWidthInTiles,
                          petriterm::world::kDefaultWorldHeightInTiles, seed);
        std::set<BiomeType> presentBiomes;
        world.forEachTile(
            [&](int, int, const Tile& tile) { presentBiomes.insert(tile.biome); });
        REQUIRE(presentBiomes.size() == 5);
    }
}

TEST_CASE("generated tile fields stay within their documented ranges", "[worldgen]") {
    const WorldGrid world = generateWorld(80, 30, 99);
    world.forEachTile([&](int, int, const Tile& tile) {
        REQUIRE(tile.elevationNormalized >= 0.0);
        REQUIRE(tile.elevationNormalized <= 1.0);
        REQUIRE(tile.baseTemperatureCelsius >= -15.0);
        REQUIRE(tile.baseTemperatureCelsius <= 40.0);
        REQUIRE(tile.baseHumidityPercent >= 0.0);
        REQUIRE(tile.baseHumidityPercent <= 100.0);
        REQUIRE(tile.surfaceWaterLevel >= 0.0);
        REQUIRE(tile.surfaceWaterLevel <= 1.0);
        REQUIRE(tile.currentTemperatureCelsius == tile.baseTemperatureCelsius);
        REQUIRE(tile.currentHumidityPercent == tile.baseHumidityPercent);
    });
}

TEST_CASE("classifyBiomeFromClimate follows the documented rules", "[worldgen]") {
    REQUIRE(classifyBiomeFromClimate(-5.0, 50.0, 0.5) == BiomeType::Tundra);
    REQUIRE(classifyBiomeFromClimate(2.9, 90.0, 0.1) == BiomeType::Tundra);
    REQUIRE(classifyBiomeFromClimate(18.0, 80.0, 0.2) == BiomeType::Wetland);
    REQUIRE(classifyBiomeFromClimate(30.0, 80.0, 0.2) == BiomeType::Wetland);
    REQUIRE(classifyBiomeFromClimate(30.0, 80.0, 0.6) == BiomeType::TropicalJungle);
    REQUIRE(classifyBiomeFromClimate(30.0, 20.0, 0.5) == BiomeType::Desert);
    REQUIRE(classifyBiomeFromClimate(15.0, 50.0, 0.5) == BiomeType::Grassland);
    REQUIRE(classifyBiomeFromClimate(30.0, 45.0, 0.5) == BiomeType::Grassland);
}
