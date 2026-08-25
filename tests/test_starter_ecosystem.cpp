#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "petriterm/engine/RandomNumberGenerator.hpp"
#include "petriterm/game/StarterEcosystem.hpp"
#include "petriterm/organisms/Organism.hpp"
#include "petriterm/organisms/Species.hpp"
#include "petriterm/world/WorldGrid.hpp"

using petriterm::engine::RandomNumberGenerator;
using petriterm::game::kStarterDetritusPerTile;
using petriterm::game::seedStarterEcosystem;
using petriterm::game::StarterDensityPerHundredTiles;
using petriterm::organisms::OrganismCategory;
using petriterm::organisms::Species;
using petriterm::world::WorldGrid;

namespace {

constexpr double kWorldTemperatureCelsius = 20.0;
constexpr double kWorldHumidityPercent = 50.0;

/// A species of the given category that is comfortable in the test world.
Species makeSpecies(const char* speciesId, OrganismCategory category) {
    Species species;
    species.speciesId = speciesId;
    species.category = category;
    species.traits.idealTemperatureCelsius = kWorldTemperatureCelsius;
    species.traits.temperatureToleranceRange = 15.0;
    species.traits.idealHumidityPercent = kWorldHumidityPercent;
    species.traits.humidityToleranceRange = 30.0;
    species.traits.energyGainedPerFeeding = 4.0;
    species.traits.energyConsumedPerTick = 1.0;
    species.traits.energyRequiredToReproduce = 12.0;
    species.traits.reproductionEnergyCost = 6.0;
    species.traits.maximumAgeInTicks = 1000;
    return species;
}

WorldGrid makeUniformWorld(int widthInTiles, int heightInTiles) {
    WorldGrid world(widthInTiles, heightInTiles);
    world.forEachTile([](int, int, petriterm::world::Tile& tile) {
        tile.currentTemperatureCelsius = kWorldTemperatureCelsius;
        tile.currentHumidityPercent = kWorldHumidityPercent;
    });
    return world;
}

/// Counts the living organisms of the given category across the world.
int countCategory(const WorldGrid& world, OrganismCategory category) {
    int total = 0;
    world.forEachTile([&](int, int, const petriterm::world::Tile& tile) {
        for (const auto& occupant : tile.occupyingOrganisms) {
            if (occupant->isAlive && occupant->species->category == category) {
                ++total;
            }
        }
    });
    return total;
}

/// Describes where everything ended up, as a comparable summary of a seeding.
std::vector<int> layoutFingerprint(const WorldGrid& world) {
    std::vector<int> fingerprint;
    world.forEachTile(
        [&](int columnIndex, int rowIndex, const petriterm::world::Tile& tile) {
            for (const auto& occupant : tile.occupyingOrganisms) {
                fingerprint.push_back(columnIndex);
                fingerprint.push_back(rowIndex);
                fingerprint.push_back(static_cast<int>(occupant->species->category));
            }
        });
    return fingerprint;
}

}

TEST_CASE("a starter ecosystem seeds every trophic level", "[starter]") {
    const Species plant = makeSpecies("p", OrganismCategory::Plant);
    const Species herbivore = makeSpecies("h", OrganismCategory::Herbivore);
    const Species carnivore = makeSpecies("c", OrganismCategory::Carnivore);
    const Species omnivore = makeSpecies("o", OrganismCategory::Omnivore);
    const Species decomposer = makeSpecies("d", OrganismCategory::Decomposer);
    const std::vector<const Species*> palette{&plant, &herbivore, &carnivore, &omnivore,
                                              &decomposer};

    WorldGrid world = makeUniformWorld(40, 40);
    RandomNumberGenerator random(5);
    seedStarterEcosystem(world, palette, random);

    for (const OrganismCategory category :
         {OrganismCategory::Plant, OrganismCategory::Herbivore, OrganismCategory::Carnivore,
          OrganismCategory::Omnivore, OrganismCategory::Decomposer}) {
        REQUIRE(countCategory(world, category) > 0);
    }

    // The pyramid shape matters as much as the presence: more producers than
    // grazers, more grazers than predators.
    REQUIRE(countCategory(world, OrganismCategory::Plant) >
            countCategory(world, OrganismCategory::Herbivore));
    REQUIRE(countCategory(world, OrganismCategory::Herbivore) >
            countCategory(world, OrganismCategory::Carnivore));
}

TEST_CASE("starter detritus is seeded so decomposers have something to eat", "[starter]") {
    // Without this, decomposers starve before the first organism dies of old age
    // and the run loses a whole trophic level in its opening ticks.
    const Species decomposer = makeSpecies("d", OrganismCategory::Decomposer);
    WorldGrid world = makeUniformWorld(8, 8);
    RandomNumberGenerator random(5);
    seedStarterEcosystem(world, {&decomposer}, random);

    world.forEachTile([](int, int, const petriterm::world::Tile& tile) {
        REQUIRE(tile.detritusLevel == Catch::Approx(kStarterDetritusPerTile));
    });
}

TEST_CASE("one seed reproduces one starting world", "[starter][determinism]") {
    const Species plant = makeSpecies("p", OrganismCategory::Plant);
    const Species herbivore = makeSpecies("h", OrganismCategory::Herbivore);
    const std::vector<const Species*> palette{&plant, &herbivore};

    const auto seedWithGenerator = [&palette](std::uint64_t seed) {
        WorldGrid world = makeUniformWorld(20, 20);
        RandomNumberGenerator random(seed);
        seedStarterEcosystem(world, palette, random);
        return layoutFingerprint(world);
    };

    REQUIRE(seedWithGenerator(11) == seedWithGenerator(11));
    // And a different seed has to actually place things differently, or the check
    // above proves nothing.
    REQUIRE(seedWithGenerator(11) != seedWithGenerator(12));
}

TEST_CASE("nothing is seeded onto ground it cannot survive", "[starter]") {
    // A species whose band excludes the whole world should simply not appear,
    // rather than being placed somewhere it starves in the opening ticks.
    Species arcticSpecies = makeSpecies("arctic", OrganismCategory::Plant);
    arcticSpecies.traits.idealTemperatureCelsius = -40.0;
    arcticSpecies.traits.temperatureToleranceRange = 5.0;

    WorldGrid world = makeUniformWorld(20, 20);
    RandomNumberGenerator random(5);
    seedStarterEcosystem(world, {&arcticSpecies}, random);

    REQUIRE(countCategory(world, OrganismCategory::Plant) == 0);
}

TEST_CASE("density scales the seeded population with the world", "[starter]") {
    const Species plant = makeSpecies("p", OrganismCategory::Plant);
    StarterDensityPerHundredTiles density;
    density.plants = 10.0;

    WorldGrid smallWorld = makeUniformWorld(10, 10);
    RandomNumberGenerator smallRandom(5);
    seedStarterEcosystem(smallWorld, {&plant}, smallRandom, density);

    WorldGrid largeWorld = makeUniformWorld(20, 20);
    RandomNumberGenerator largeRandom(5);
    seedStarterEcosystem(largeWorld, {&plant}, largeRandom, density);

    // Ten per hundred tiles: ten on a hundred-tile world, forty on four hundred.
    REQUIRE(countCategory(smallWorld, OrganismCategory::Plant) == 10);
    REQUIRE(countCategory(largeWorld, OrganismCategory::Plant) == 40);
}

TEST_CASE("a small world still gets a predator", "[starter]") {
    // Rounding a fractional density down to zero would quietly drop the top of the
    // food web on any world small enough for a test to run quickly.
    const Species carnivore = makeSpecies("c", OrganismCategory::Carnivore);
    StarterDensityPerHundredTiles density;
    density.carnivores = 0.25;

    WorldGrid world = makeUniformWorld(8, 8);
    RandomNumberGenerator random(5);
    seedStarterEcosystem(world, {&carnivore}, random, density);

    REQUIRE(countCategory(world, OrganismCategory::Carnivore) >= 1);
}
