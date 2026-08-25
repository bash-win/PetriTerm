#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "petriterm/engine/RandomNumberGenerator.hpp"
#include "petriterm/organisms/Organism.hpp"
#include "petriterm/organisms/Species.hpp"
#include "petriterm/simulation/EcosystemCensus.hpp"
#include "petriterm/simulation/SimulationEngine.hpp"
#include "petriterm/world/WorldGrid.hpp"

using petriterm::engine::RandomNumberGenerator;
using petriterm::organisms::Organism;
using petriterm::organisms::OrganismCategory;
using petriterm::organisms::Species;
using petriterm::simulation::censusCsvHeader;
using petriterm::simulation::EcosystemCensus;
using petriterm::simulation::formatCensusAsCsvRow;
using petriterm::simulation::sampleEcosystemCensus;
using petriterm::simulation::SimulationEngine;
using petriterm::world::WorldGrid;

namespace {

/// A plant comfortable at the world climate the tests below build.
Species makePlant() {
    Species species;
    species.speciesId = "test_grass";
    species.category = OrganismCategory::Plant;
    species.traits.idealTemperatureCelsius = 20.0;
    species.traits.temperatureToleranceRange = 10.0;
    species.traits.idealHumidityPercent = 50.0;
    species.traits.humidityToleranceRange = 20.0;
    species.traits.energyGainedPerFeeding = 4.0;
    species.traits.energyConsumedPerTick = 1.0;
    species.traits.energyRequiredToReproduce = 12.0;
    species.traits.reproductionEnergyCost = 6.0;
    species.traits.maximumAgeInTicks = 100000;
    return species;
}

/// A world at the plant's ideal climate with the given soil and detritus on every
/// tile.
WorldGrid makeWorld(int widthInTiles, int heightInTiles, double soilNutrientLevel,
                    double detritusLevel) {
    WorldGrid world(widthInTiles, heightInTiles);
    world.forEachTile([&](int, int, petriterm::world::Tile& tile) {
        tile.baseTemperatureCelsius = 20.0;
        tile.baseHumidityPercent = 50.0;
        tile.currentTemperatureCelsius = 20.0;
        tile.currentHumidityPercent = 50.0;
        tile.soilNutrientLevel = soilNutrientLevel;
        tile.detritusLevel = detritusLevel;
    });
    return world;
}

/// Returns how many comma-separated fields the text has.
int commaSeparatedFieldCount(const std::string& text) {
    return 1 + static_cast<int>(std::count(text.begin(), text.end(), ','));
}

}

TEST_CASE("a census reports populations and fertility for the sampled tick", "[census]") {
    const Species plantSpecies = makePlant();
    WorldGrid world = makeWorld(4, 4, 0.5, 2.0);
    for (int index = 0; index < 3; ++index) {
        world.tileAt(index, 0).occupyingOrganisms.push_back(
            std::make_unique<Organism>(&plantSpecies, index, 0));
    }

    RandomNumberGenerator random(1);
    SimulationEngine simulation(std::move(world), random);
    simulation.advanceOneTick();

    const EcosystemCensus census = sampleEcosystemCensus(simulation);
    REQUIRE(census.tickIndex == 1);
    REQUIRE(census.tileCount == 16);
    REQUIRE(census.livingCountOf(OrganismCategory::Plant) == 3);
    REQUIRE(census.totalLivingCount == 3);
    // Detritus was seeded on every tile and nothing has died, so it is untouched.
    REQUIRE(census.meanDetritusLevel() == Catch::Approx(2.0));
    // The three plants photosynthesized, so their tiles are drawn down a little
    // and the mean sits just under what every tile started with.
    REQUIRE(census.meanSoilNutrientLevel() < 0.5);
    REQUIRE(census.meanSoilNutrientLevel() > 0.49);
}

TEST_CASE("a census agrees with the tick report it was sampled from", "[census]") {
    // The census must never disagree with the engine about what happened, so it
    // reads the report rather than recounting the world.
    const Species plantSpecies = makePlant();
    WorldGrid world = makeWorld(3, 3, 1.0, 0.0);
    world.tileAt(1, 1).occupyingOrganisms.push_back(
        std::make_unique<Organism>(&plantSpecies, 1, 1));

    RandomNumberGenerator random(2);
    SimulationEngine simulation(std::move(world), random);

    for (int tick = 0; tick < 5; ++tick) {
        simulation.advanceOneTick();
        const EcosystemCensus census = sampleEcosystemCensus(simulation);
        REQUIRE(census.tickIndex == simulation.tickIndex());
        REQUIRE(census.totalLivingCount == simulation.latestTickReport().totalLivingCount);
        REQUIRE(census.birthCount == simulation.latestTickReport().birthCount);
        REQUIRE(census.deathCount == simulation.latestTickReport().deathCount);
        REQUIRE(census.feedingCount == simulation.latestTickReport().feedingCount);
    }
}

TEST_CASE("every trophic category has to be present for a food web to count", "[census]") {
    EcosystemCensus census;
    REQUIRE_FALSE(census.hasEveryTrophicCategory());

    for (int index = 0; index < petriterm::organisms::kOrganismCategoryCount; ++index) {
        census.livingCountByCategory[static_cast<std::size_t>(index)] = 1;
    }
    REQUIRE(census.hasEveryTrophicCategory());

    // Losing any single level breaks it, including the bottom one.
    census.livingCountByCategory[0] = 0;
    REQUIRE_FALSE(census.hasEveryTrophicCategory());
}

TEST_CASE("means are zero rather than a division by zero for an empty world", "[census]") {
    const EcosystemCensus census;
    REQUIRE(census.tileCount == 0);
    REQUIRE(census.meanSoilNutrientLevel() == Catch::Approx(0.0));
    REQUIRE(census.meanDetritusLevel() == Catch::Approx(0.0));
    REQUIRE(census.meanCurrentTemperatureCelsius() == Catch::Approx(0.0));
}

TEST_CASE("the CSV row has exactly as many fields as the header names", "[census]") {
    // The reason to test this at all: a column added to one and not the other
    // silently misaligns every downstream reading of the output.
    const EcosystemCensus census;
    REQUIRE(commaSeparatedFieldCount(formatCensusAsCsvRow(census)) ==
            commaSeparatedFieldCount(censusCsvHeader()));
}

TEST_CASE("a CSV row carries no newline of its own", "[census]") {
    // The caller decides the line ending, so neither the header nor a row may
    // bring one along or the output grows blank lines.
    const EcosystemCensus census;
    REQUIRE(censusCsvHeader().find('\n') == std::string::npos);
    REQUIRE(formatCensusAsCsvRow(census).find('\n') == std::string::npos);
}
