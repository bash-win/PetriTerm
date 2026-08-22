#include <memory>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "petriterm/engine/RandomNumberGenerator.hpp"
#include "petriterm/organisms/Organism.hpp"
#include "petriterm/organisms/Species.hpp"
#include "petriterm/simulation/SimulationEngine.hpp"
#include "petriterm/world/WorldGrid.hpp"

using petriterm::engine::RandomNumberGenerator;
using petriterm::organisms::Organism;
using petriterm::organisms::OrganismCategory;
using petriterm::organisms::Species;
using petriterm::simulation::environmentalFitness;
using petriterm::simulation::SimulationEngine;
using petriterm::simulation::TickReport;
using petriterm::world::WorldGrid;

namespace {

constexpr double kIdealTemperatureCelsius = 20.0;
constexpr double kIdealHumidityPercent = 50.0;

/// A sessile producer sitting at the shared ideal climate. Seed dispersal is off
/// by default so a test only sees births when it asks for them.
Species makePlant() {
    Species species;
    species.speciesId = "test_grass";
    species.category = OrganismCategory::Plant;
    species.traits.idealTemperatureCelsius = kIdealTemperatureCelsius;
    species.traits.temperatureToleranceRange = 10.0;
    species.traits.idealHumidityPercent = kIdealHumidityPercent;
    species.traits.humidityToleranceRange = 20.0;
    species.traits.energyGainedPerFeeding = 4.0;
    species.traits.energyConsumedPerTick = 1.0;
    species.traits.energyRequiredToReproduce = 12.0;
    species.traits.reproductionEnergyCost = 6.0;
    species.traits.reproductionCooldownTicks = 5;
    species.traits.maximumAgeInTicks = 100000;
    species.traits.movementRangeInTiles = 0;
    species.traits.seedDispersalProbabilityPerTick = 0.0;
    return species;
}

/// A grazer that eats plants and can reach two tiles.
Species makeHerbivore() {
    Species species;
    species.speciesId = "test_rabbit";
    species.category = OrganismCategory::Herbivore;
    species.traits.idealTemperatureCelsius = kIdealTemperatureCelsius;
    species.traits.temperatureToleranceRange = 10.0;
    species.traits.idealHumidityPercent = kIdealHumidityPercent;
    species.traits.humidityToleranceRange = 20.0;
    species.traits.energyGainedPerFeeding = 8.0;
    species.traits.energyConsumedPerTick = 2.0;
    species.traits.energyRequiredToReproduce = 20.0;
    species.traits.reproductionEnergyCost = 10.0;
    species.traits.reproductionCooldownTicks = 8;
    species.traits.maximumAgeInTicks = 100000;
    species.traits.movementRangeInTiles = 2;
    species.diet.allowCategory(OrganismCategory::Plant);
    return species;
}

/// A predator that eats herbivores.
Species makeCarnivore() {
    Species species = makeHerbivore();
    species.speciesId = "test_fox";
    species.category = OrganismCategory::Carnivore;
    species.diet = {};
    species.diet.allowCategory(OrganismCategory::Herbivore);
    species.traits.energyGainedPerFeeding = 16.0;
    return species;
}

/// A scavenger. Its diet stays empty: it works corpses, not the living.
Species makeDecomposer() {
    Species species = makePlant();
    species.speciesId = "test_beetle";
    species.category = OrganismCategory::Decomposer;
    species.traits.energyGainedPerFeeding = 5.0;
    species.traits.movementRangeInTiles = 1;
    return species;
}

/// A world whose every tile sits at the given climate. Base values matter as much
/// as current ones: the engine re-derives current climate from the base each tick,
/// so a test that sets only the current values would see them overwritten.
WorldGrid makeUniformWorld(int widthInTiles, int heightInTiles,
                           double temperatureCelsius = kIdealTemperatureCelsius,
                           double humidityPercent = kIdealHumidityPercent) {
    WorldGrid world(widthInTiles, heightInTiles);
    world.forEachTile([&](int, int, petriterm::world::Tile& tile) {
        tile.baseTemperatureCelsius = temperatureCelsius;
        tile.baseHumidityPercent = humidityPercent;
        tile.currentTemperatureCelsius = temperatureCelsius;
        tile.currentHumidityPercent = humidityPercent;
    });
    return world;
}

/// Adds an organism of the species to the tile and returns a reference to it.
Organism& placeOrganism(WorldGrid& world, const Species& species, int columnIndex,
                        int rowIndex) {
    auto& occupants = world.tileAt(columnIndex, rowIndex).occupyingOrganisms;
    occupants.push_back(std::make_unique<Organism>(&species, columnIndex, rowIndex));
    return *occupants.back();
}

/// Tolerance for an expected energy total. The climate phase advances the season
/// before anything feeds, so by the time an organism eats it sits a fraction of a
/// degree off the ideal and its fitness is just under 1.0. That drift is the
/// simulation working correctly, so the arithmetic below allows for it rather than
/// pinning temperatures to defeat it.
constexpr double kSeasonalDriftMargin = 0.1;

/// Returns every living organism in the world, in row-major tile order.
std::vector<const Organism*> livingOrganisms(const WorldGrid& world) {
    std::vector<const Organism*> living;
    world.forEachTile([&living](int, int, const petriterm::world::Tile& tile) {
        for (const auto& occupant : tile.occupyingOrganisms) {
            if (occupant->isAlive) {
                living.push_back(occupant.get());
            }
        }
    });
    return living;
}

}

TEST_CASE("fitness peaks at the ideal climate and vanishes outside tolerance",
          "[simulation][fitness]") {
    const Species species = makePlant();

    SECTION("exactly at the ideal") {
        REQUIRE(environmentalFitness(species.traits, kIdealTemperatureCelsius,
                                     kIdealHumidityPercent) == Catch::Approx(1.0));
    }

    SECTION("halfway out on one axis scales that axis only") {
        REQUIRE(environmentalFitness(species.traits, kIdealTemperatureCelsius + 5.0,
                                     kIdealHumidityPercent) == Catch::Approx(0.5));
        REQUIRE(environmentalFitness(species.traits, kIdealTemperatureCelsius - 5.0,
                                     kIdealHumidityPercent) == Catch::Approx(0.5));
    }

    SECTION("the two axes multiply") {
        REQUIRE(environmentalFitness(species.traits, kIdealTemperatureCelsius + 5.0,
                                     kIdealHumidityPercent + 10.0) == Catch::Approx(0.25));
    }

    SECTION("at or beyond the tolerance edge nothing is left") {
        REQUIRE(environmentalFitness(species.traits, kIdealTemperatureCelsius + 10.0,
                                     kIdealHumidityPercent) == Catch::Approx(0.0));
        REQUIRE(environmentalFitness(species.traits, kIdealTemperatureCelsius + 500.0,
                                     kIdealHumidityPercent) == Catch::Approx(0.0));
    }

    SECTION("a zero-width tolerance band does not divide by zero") {
        Species brittle = makePlant();
        brittle.traits.temperatureToleranceRange = 0.0;
        brittle.traits.humidityToleranceRange = 0.0;
        REQUIRE(environmentalFitness(brittle.traits, kIdealTemperatureCelsius,
                                     kIdealHumidityPercent) == Catch::Approx(1.0));
        REQUIRE(environmentalFitness(brittle.traits, kIdealTemperatureCelsius + 0.5,
                                     kIdealHumidityPercent) == Catch::Approx(0.0));
    }
}

TEST_CASE("the tick index counts simulated ticks", "[simulation]") {
    RandomNumberGenerator random(1);
    SimulationEngine simulation(makeUniformWorld(4, 4), random);
    REQUIRE(simulation.tickIndex() == 0);
    REQUIRE(simulation.latestTickReport().totalLivingCount == 0);

    simulation.advanceOneTick();
    simulation.advanceOneTick();
    REQUIRE(simulation.tickIndex() == 2);
}

TEST_CASE("metabolism runs every tick and the dead are cleared", "[simulation]") {
    const Species species = makePlant();
    WorldGrid world = makeUniformWorld(3, 3);
    // Far outside its temperature band, so photosynthesis yields nothing and the
    // plant can only burn what it started with.
    world.forEachTile([](int, int, petriterm::world::Tile& tile) {
        tile.baseTemperatureCelsius = kIdealTemperatureCelsius + 50.0;
    });
    placeOrganism(world, species, 1, 1);

    RandomNumberGenerator random(1);
    SimulationEngine simulation(std::move(world), random);

    const TickReport& firstTick = simulation.advanceOneTick();
    REQUIRE(firstTick.feedingCount == 0);
    REQUIRE(firstTick.totalLivingCount == 1);
    REQUIRE(livingOrganisms(simulation.world()).front()->remainingEnergyUnits ==
            Catch::Approx(5.0));

    // Starting energy is half the reproduction threshold, so six ticks of upkeep
    // at one unit each is enough to finish it.
    int deathsSeen = firstTick.deathCount;
    for (int tick = 0; tick < 6; ++tick) {
        deathsSeen += simulation.advanceOneTick().deathCount;
    }
    REQUIRE(deathsSeen == 1);
    REQUIRE(simulation.latestTickReport().totalLivingCount == 0);
    REQUIRE(livingOrganisms(simulation.world()).empty());
}

TEST_CASE("a plant at its ideal climate photosynthesizes and survives", "[simulation]") {
    const Species species = makePlant();
    WorldGrid world = makeUniformWorld(3, 3);
    placeOrganism(world, species, 1, 1);

    RandomNumberGenerator random(1);
    SimulationEngine simulation(std::move(world), random);

    const TickReport& report = simulation.advanceOneTick();
    REQUIRE(report.feedingCount == 1);
    // Six to start, minus one upkeep, plus four from a near-full-fitness feeding.
    REQUIRE(livingOrganisms(simulation.world()).front()->remainingEnergyUnits ==
            Catch::Approx(9.0).margin(kSeasonalDriftMargin));

    // Fifty ticks is inside the opening stretch of Clear weather, so the only
    // climate drift is seasonal and the plant stays comfortably inside its band.
    // Surviving a heatwave landing on top of high summer is a balance question for
    // the tuning pass, not something this test should assume either way.
    for (int tick = 0; tick < 50; ++tick) {
        simulation.advanceOneTick();
    }
    REQUIRE(simulation.latestTickReport().totalLivingCount == 1);
    REQUIRE(simulation.latestTickReport().birthCount == 0);
    // Comfortably above the six units it started with: photosynthesis is covering
    // upkeep several times over.
    REQUIRE(livingOrganisms(simulation.world()).front()->remainingEnergyUnits > 20.0);
}

TEST_CASE("energy is capped at a multiple of the reproduction threshold", "[simulation]") {
    Species species = makePlant();
    // A yield far past the ceiling, so one feeding is enough to prove the clamp
    // rather than leaving the result at the mercy of the weather.
    species.traits.energyGainedPerFeeding = 1000.0;
    WorldGrid world = makeUniformWorld(1, 1);
    placeOrganism(world, species, 0, 0);

    RandomNumberGenerator random(1);
    SimulationEngine simulation(std::move(world), random);
    simulation.advanceOneTick();

    // Twice the twelve-unit reproduction threshold, exactly.
    REQUIRE(livingOrganisms(simulation.world()).front()->remainingEnergyUnits ==
            Catch::Approx(24.0));
}

TEST_CASE("a plant disperses a seed onto a tile with room", "[simulation]") {
    Species species = makePlant();
    species.traits.seedDispersalProbabilityPerTick = 1.0;
    WorldGrid world = makeUniformWorld(3, 3);
    Organism& parent = placeOrganism(world, species, 1, 1);
    parent.remainingEnergyUnits = 20.0;
    parent.ticksUntilCanReproduce = 0;
    // Organisms are heap-owned by their tile, so handing the world to the engine
    // moves the owning vectors but leaves the organisms themselves where they are.
    // Holding the parent's address is the only way to tell it from its offspring,
    // which can be seeded onto an earlier tile in row-major order.
    const Organism* parentAddress = &parent;

    RandomNumberGenerator random(1);
    SimulationEngine simulation(std::move(world), random);

    const TickReport& report = simulation.advanceOneTick();
    REQUIRE(report.birthCount == 1);
    REQUIRE(report.totalLivingCount == 2);

    REQUIRE(parentAddress->ticksUntilCanReproduce == 5);
    // Twenty to start, minus one upkeep, plus four from feeding, minus the six the
    // offspring cost.
    REQUIRE(parentAddress->remainingEnergyUnits ==
            Catch::Approx(17.0).margin(kSeasonalDriftMargin));
}

TEST_CASE("a plant with no seed dispersal never reproduces", "[simulation]") {
    const Species species = makePlant();
    WorldGrid world = makeUniformWorld(3, 3);
    Organism& parent = placeOrganism(world, species, 1, 1);
    parent.remainingEnergyUnits = 20.0;
    parent.ticksUntilCanReproduce = 0;

    RandomNumberGenerator random(1);
    SimulationEngine simulation(std::move(world), random);
    for (int tick = 0; tick < 100; ++tick) {
        REQUIRE(simulation.advanceOneTick().birthCount == 0);
    }
}

TEST_CASE("births stop at the tile's carrying capacity", "[simulation]") {
    Species species = makePlant();
    species.traits.seedDispersalProbabilityPerTick = 1.0;
    // A one-tile world, filled to the four-plant capacity, leaves a well-fed
    // parent nowhere to put an offspring.
    WorldGrid world = makeUniformWorld(1, 1);
    for (int index = 0; index < 4; ++index) {
        Organism& plant = placeOrganism(world, species, 0, 0);
        plant.remainingEnergyUnits = 20.0;
        plant.ticksUntilCanReproduce = 0;
    }

    RandomNumberGenerator random(1);
    SimulationEngine simulation(std::move(world), random);
    for (int tick = 0; tick < 50; ++tick) {
        REQUIRE(simulation.advanceOneTick().birthCount == 0);
    }
    REQUIRE(simulation.latestTickReport().totalLivingCount == 4);
}

TEST_CASE("a herbivore grazes a plant without killing it", "[simulation]") {
    const Species plantSpecies = makePlant();
    const Species herbivoreSpecies = makeHerbivore();
    WorldGrid world = makeUniformWorld(3, 3);
    Organism& plant = placeOrganism(world, plantSpecies, 1, 1);
    plant.remainingEnergyUnits = 20.0;
    placeOrganism(world, herbivoreSpecies, 1, 1);

    RandomNumberGenerator random(1);
    SimulationEngine simulation(std::move(world), random);

    const TickReport& report = simulation.advanceOneTick();
    REQUIRE(report.feedingCount >= 1);
    REQUIRE(report.deathCount == 0);
    REQUIRE(report.livingCountOf(OrganismCategory::Plant) == 1);
    REQUIRE(report.livingCountOf(OrganismCategory::Herbivore) == 1);

    // Grazing is capped at half the plant's remaining energy, and the plant's own
    // upkeep and photosynthesis land in the same tick.
    const std::vector<const Organism*> survivors = livingOrganisms(simulation.world());
    const Organism* grazedPlant =
        survivors.front()->species->category == OrganismCategory::Plant ? survivors.front()
                                                                        : survivors.back();
    const Organism* grazer =
        survivors.front() == grazedPlant ? survivors.back() : survivors.front();
    REQUIRE(grazedPlant->remainingEnergyUnits < 20.0);
    REQUIRE(grazedPlant->remainingEnergyUnits > 0.0);
    REQUIRE(grazer->remainingEnergyUnits > 10.0);
}

TEST_CASE("a herbivore closes on prey within its reach", "[simulation]") {
    const Species plantSpecies = makePlant();
    const Species herbivoreSpecies = makeHerbivore();
    WorldGrid world = makeUniformWorld(5, 5);
    Organism& plant = placeOrganism(world, plantSpecies, 3, 1);
    plant.remainingEnergyUnits = 20.0;
    placeOrganism(world, herbivoreSpecies, 1, 1);

    RandomNumberGenerator random(1);
    SimulationEngine simulation(std::move(world), random);
    simulation.advanceOneTick();

    // Reach is two tiles, so the plant two columns away is found and closed on.
    REQUIRE(simulation.world().tileAt(1, 1).livingOrganismCount() == 0);
    REQUIRE(simulation.world().tileAt(3, 1).livingOrganismCount() == 2);
}

TEST_CASE("a sessile species never leaves its tile", "[simulation]") {
    const Species species = makePlant();
    WorldGrid world = makeUniformWorld(5, 5);
    placeOrganism(world, species, 2, 2);

    RandomNumberGenerator random(7);
    SimulationEngine simulation(std::move(world), random);
    for (int tick = 0; tick < 200; ++tick) {
        simulation.advanceOneTick();
    }
    REQUIRE(simulation.world().tileAt(2, 2).livingOrganismCount() == 1);
}

TEST_CASE("a carnivore kills its prey outright", "[simulation]") {
    const Species herbivoreSpecies = makeHerbivore();
    const Species carnivoreSpecies = makeCarnivore();
    WorldGrid world = makeUniformWorld(3, 3);
    placeOrganism(world, herbivoreSpecies, 1, 1);
    placeOrganism(world, carnivoreSpecies, 1, 1);

    RandomNumberGenerator random(1);
    SimulationEngine simulation(std::move(world), random);

    const TickReport& report = simulation.advanceOneTick();
    REQUIRE(report.deathCount == 1);
    REQUIRE(report.livingCountOf(OrganismCategory::Herbivore) == 0);
    REQUIRE(report.livingCountOf(OrganismCategory::Carnivore) == 1);
    // Ten to start, minus two upkeep, plus a sixteen-unit meal.
    REQUIRE(livingOrganisms(simulation.world()).front()->remainingEnergyUnits ==
            Catch::Approx(24.0).margin(kSeasonalDriftMargin));
}

TEST_CASE("a decomposer feeds on a corpse left by this tick's deaths", "[simulation]") {
    const Species plantSpecies = makePlant();
    const Species decomposerSpecies = makeDecomposer();
    WorldGrid world = makeUniformWorld(3, 3);
    // One unit of energy left and one unit of upkeep, so it dies in the metabolism
    // phase and is still lying there when the decomposer acts.
    Organism& dying = placeOrganism(world, plantSpecies, 1, 1);
    dying.remainingEnergyUnits = 1.0;
    Organism& decomposer = placeOrganism(world, decomposerSpecies, 1, 1);
    decomposer.remainingEnergyUnits = 6.0;

    RandomNumberGenerator random(1);
    SimulationEngine simulation(std::move(world), random);

    const TickReport& report = simulation.advanceOneTick();
    REQUIRE(report.deathCount == 1);
    REQUIRE(report.livingCountOf(OrganismCategory::Decomposer) == 1);
    // Six to start, minus one upkeep, plus a five-unit meal off the corpse.
    REQUIRE(livingOrganisms(simulation.world()).front()->remainingEnergyUnits ==
            Catch::Approx(10.0).margin(kSeasonalDriftMargin));
}

TEST_CASE("the same seed reproduces the same run", "[simulation][determinism]") {
    Species plantSpecies = makePlant();
    plantSpecies.traits.seedDispersalProbabilityPerTick = 0.2;
    const Species herbivoreSpecies = makeHerbivore();
    const Species carnivoreSpecies = makeCarnivore();

    const auto buildWorld = [&]() {
        WorldGrid world = makeUniformWorld(12, 12);
        for (int index = 0; index < 6; ++index) {
            placeOrganism(world, plantSpecies, index, index);
        }
        placeOrganism(world, herbivoreSpecies, 4, 6);
        placeOrganism(world, herbivoreSpecies, 8, 2);
        placeOrganism(world, carnivoreSpecies, 6, 6);
        return world;
    };

    const auto runTrajectory = [&](std::uint64_t seed) {
        RandomNumberGenerator random(seed);
        SimulationEngine simulation(buildWorld(), random);
        std::vector<int> populationByTick;
        for (int tick = 0; tick < 400; ++tick) {
            populationByTick.push_back(simulation.advanceOneTick().totalLivingCount);
        }
        return populationByTick;
    };

    const std::vector<int> first = runTrajectory(2024);
    const std::vector<int> second = runTrajectory(2024);
    REQUIRE(first == second);

    // A different seed has to actually diverge, or the test above proves nothing.
    REQUIRE(runTrajectory(99) != first);
}
