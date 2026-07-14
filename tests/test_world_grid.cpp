#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

#include "petriterm/world/Biome.hpp"
#include "petriterm/world/WorldGrid.hpp"

using petriterm::world::BiomeType;
using petriterm::world::describeBiome;
using petriterm::world::Tile;
using petriterm::world::WorldGrid;

namespace {

constexpr BiomeType kAllBiomes[] = {BiomeType::TropicalJungle, BiomeType::Desert,
                                    BiomeType::Grassland, BiomeType::Wetland,
                                    BiomeType::Tundra};

}

TEST_CASE("describeBiome returns the matching descriptor for every biome", "[biome]") {
    for (const BiomeType biome : kAllBiomes) {
        const auto& descriptor = describeBiome(biome);
        REQUIRE(descriptor.type == biome);
        REQUIRE_FALSE(descriptor.displayName.empty());
        REQUIRE(descriptor.soilFertilityBaseline >= 0.0);
        REQUIRE(descriptor.soilFertilityBaseline <= 1.0);
    }
}

TEST_CASE("biome display names are unique", "[biome]") {
    std::set<std::string> displayNames;
    for (const BiomeType biome : kAllBiomes) {
        displayNames.insert(describeBiome(biome).displayName);
    }
    REQUIRE(displayNames.size() == std::size(kAllBiomes));
}

TEST_CASE("WorldGrid reports its dimensions and default-initializes tiles", "[worldgrid]") {
    const WorldGrid world(6, 4);
    REQUIRE(world.widthInTiles() == 6);
    REQUIRE(world.heightInTiles() == 4);
    REQUIRE(world.tileAt(0, 0).biome == BiomeType::Grassland);
    REQUIRE(world.tileAt(5, 3).soilNutrientLevel == 0.0);
}

TEST_CASE("WorldGrid tileAt returns writable tiles", "[worldgrid]") {
    WorldGrid world(6, 4);
    world.tileAt(2, 1).biome = BiomeType::Desert;
    world.tileAt(2, 1).currentTemperatureCelsius = 41.5;
    REQUIRE(world.tileAt(2, 1).biome == BiomeType::Desert);
    REQUIRE(world.tileAt(2, 1).currentTemperatureCelsius == 41.5);
    REQUIRE(world.tileAt(1, 2).biome == BiomeType::Grassland);
}

TEST_CASE("WorldGrid containsCoordinate respects bounds", "[worldgrid]") {
    const WorldGrid world(6, 4);
    REQUIRE(world.containsCoordinate(0, 0));
    REQUIRE(world.containsCoordinate(5, 3));
    REQUIRE_FALSE(world.containsCoordinate(6, 0));
    REQUIRE_FALSE(world.containsCoordinate(0, 4));
    REQUIRE_FALSE(world.containsCoordinate(-1, -1));
}

TEST_CASE("forEachTile visits every tile exactly once and can mutate", "[worldgrid]") {
    WorldGrid world(6, 4);
    int visitCount = 0;
    world.forEachTile([&](int, int, Tile& tile) {
        tile.soilNutrientLevel += 1.0;
        ++visitCount;
    });
    REQUIRE(visitCount == 24);

    const WorldGrid& readOnlyWorld = world;
    int nutrientTotal = 0;
    readOnlyWorld.forEachTile([&](int, int, const Tile& tile) {
        nutrientTotal += static_cast<int>(tile.soilNutrientLevel);
    });
    REQUIRE(nutrientTotal == 24);
}
