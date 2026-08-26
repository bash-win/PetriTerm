#include <cmath>
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

/// A scavenger. Its diet stays empty: it works the detritus a tile holds, not the
/// living.
Species makeDecomposer() {
    Species species = makePlant();
    species.speciesId = "test_beetle";
    species.category = OrganismCategory::Decomposer;
    species.traits.energyGainedPerFeeding = 5.0;
    species.traits.movementRangeInTiles = 1;
    return species;
}

/// Soil rich enough that photosynthesis runs at its full rate, so a test not
/// concerned with the nutrient cycle sees feeding limited by climate alone.
constexpr double kFullyFertileSoilNutrientLevel = 1.0;

/// Soil well below the level that saturates photosynthesis, for tests that need
/// room to watch fertility move in either direction.
constexpr double kDepletedSoilNutrientLevel = 0.2;

/// A world whose every tile sits at the given climate. Base values matter as much
/// as current ones: the engine re-derives current climate from the base each tick,
/// so a test that sets only the current values would see them overwritten. Soil
/// defaults to fertile for the same reason: a bare-soil default would make every
/// plant test a nutrient-cycle test by accident.
WorldGrid makeUniformWorld(int widthInTiles, int heightInTiles,
                           double temperatureCelsius = kIdealTemperatureCelsius,
                           double humidityPercent = kIdealHumidityPercent,
                           double soilNutrientLevel = kFullyFertileSoilNutrientLevel) {
    WorldGrid world(widthInTiles, heightInTiles);
    world.forEachTile([&](int, int, petriterm::world::Tile& tile) {
        tile.baseTemperatureCelsius = temperatureCelsius;
        tile.baseHumidityPercent = humidityPercent;
        tile.currentTemperatureCelsius = temperatureCelsius;
        tile.currentHumidityPercent = humidityPercent;
        tile.soilNutrientLevel = soilNutrientLevel;
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

    SECTION("halfway out on one axis is softened by the other still being ideal") {
        // The geometric mean of 0.5 and 1.0. Both directions, since the falloff is
        // on the distance from the ideal rather than the side of it.
        REQUIRE(environmentalFitness(species.traits, kIdealTemperatureCelsius + 5.0,
                                     kIdealHumidityPercent) ==
                Catch::Approx(std::sqrt(0.5)));
        REQUIRE(environmentalFitness(species.traits, kIdealTemperatureCelsius - 5.0,
                                     kIdealHumidityPercent) ==
                Catch::Approx(std::sqrt(0.5)));
    }

    SECTION("the two axes combine as a mean rather than compounding") {
        // Half-suited on both axes is half-suited overall, not quarter-suited. The
        // product this used to be charged a species twice for one bad tile, and a
        // weather pattern moves both axes at once, so the difference is the
        // difference between weather that stresses a map and weather that clears it.
        REQUIRE(environmentalFitness(species.traits, kIdealTemperatureCelsius + 5.0,
                                     kIdealHumidityPercent + 10.0) == Catch::Approx(0.5));
    }

    SECTION("outside either band alone is enough to yield nothing") {
        // The property the mean has to keep: a species does not get to live in the
        // wrong climate on the strength of the other axis being perfect.
        REQUIRE(environmentalFitness(species.traits, kIdealTemperatureCelsius + 20.0,
                                     kIdealHumidityPercent) == Catch::Approx(0.0));
        REQUIRE(environmentalFitness(species.traits, kIdealTemperatureCelsius,
                                     kIdealHumidityPercent + 40.0) == Catch::Approx(0.0));
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
    // Twenty ticks is four times the breeding cooldown, so every plant gets
    // several blocked attempts. The horizon stops there because nothing dies back
    // into this sealed tile, and past roughly twice that the four of them draw the
    // soil down far enough to starve - which is the nutrient cycle working, and a
    // separate test's business.
    for (int tick = 0; tick < 20; ++tick) {
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
    // Checked every tick rather than only at the end: the plant eventually
    // exhausts this sealed tile's soil and starves, and a final-state assertion
    // could not tell staying put from having died somewhere else.
    for (int tick = 0; tick < 200; ++tick) {
        simulation.advanceOneTick();
        for (const Organism* organism : livingOrganisms(simulation.world())) {
            REQUIRE(organism->tileColumnIndex == 2);
            REQUIRE(organism->tileRowIndex == 2);
        }
    }
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

TEST_CASE("a carnivore will not breed within another's territory", "[simulation]") {
    // The brake on the top of the food web. Nothing preys on a carnivore, so
    // without this its numbers answer only to prey, and they answer late enough
    // that the predators breed straight through a crash of the thing they eat.
    Species carnivoreSpecies = makeCarnivore();
    carnivoreSpecies.traits.reproductionCooldownTicks = 0;

    const auto birthsWithNeighbourAt = [&carnivoreSpecies](int neighbourColumnIndex) {
        WorldGrid world = makeUniformWorld(9, 3);
        Organism& parent = placeOrganism(world, carnivoreSpecies, 4, 1);
        parent.remainingEnergyUnits =
            carnivoreSpecies.traits.energyRequiredToReproduce * 2.0;
        if (neighbourColumnIndex >= 0) {
            placeOrganism(world, carnivoreSpecies, neighbourColumnIndex, 1);
        }
        RandomNumberGenerator random(1);
        SimulationEngine simulation(std::move(world), random);
        return simulation.advanceOneTick().birthCount;
    };

    // Alone, it breeds. With a rival on the adjoining tile, it does not. Two tiles
    // away is outside the territory and lets it breed again, which is what makes
    // this a limit that eases as the population thins rather than a flat cap.
    REQUIRE(birthsWithNeighbourAt(-1) == 1);
    REQUIRE(birthsWithNeighbourAt(5) == 0);
    REQUIRE(birthsWithNeighbourAt(6) >= 1);
}

TEST_CASE("a grazer breeds shoulder to shoulder with its own kind", "[simulation]") {
    // Territory is deliberately the hunters' alone: a herd is the herbivore's
    // answer to being eaten, and giving the grazers territory too would take that
    // away at exactly the moment they need it.
    Species herbivoreSpecies = makeHerbivore();
    herbivoreSpecies.traits.reproductionCooldownTicks = 0;

    WorldGrid world = makeUniformWorld(5, 3);
    Organism& parent = placeOrganism(world, herbivoreSpecies, 2, 1);
    parent.remainingEnergyUnits = herbivoreSpecies.traits.energyRequiredToReproduce * 2.0;
    placeOrganism(world, herbivoreSpecies, 3, 1);

    RandomNumberGenerator random(1);
    SimulationEngine simulation(std::move(world), random);
    REQUIRE(simulation.advanceOneTick().birthCount >= 1);
}

TEST_CASE("a corpse becomes detritus a decomposer can work on a later tick",
          "[simulation][nutrients]") {
    const Species plantSpecies = makePlant();
    const Species decomposerSpecies = makeDecomposer();
    // Poor soil, so mineralization has somewhere to go: on fully fertile ground it
    // would be clamped at the ceiling and invisible.
    WorldGrid world = makeUniformWorld(3, 3, kIdealTemperatureCelsius,
                                       kIdealHumidityPercent, kDepletedSoilNutrientLevel);
    // One unit of energy left and one unit of upkeep, so it dies in the metabolism
    // phase of the very first tick.
    Organism& dying = placeOrganism(world, plantSpecies, 1, 1);
    dying.remainingEnergyUnits = 1.0;
    Organism& decomposer = placeOrganism(world, decomposerSpecies, 1, 1);
    decomposer.remainingEnergyUnits = 6.0;

    RandomNumberGenerator random(1);
    SimulationEngine simulation(std::move(world), random);

    const TickReport& firstReport = simulation.advanceOneTick();
    REQUIRE(firstReport.deathCount == 1);
    REQUIRE(firstReport.livingCountOf(OrganismCategory::Decomposer) == 1);
    // Detritus is deposited in the cleanup phase, after everything has acted, so
    // the corpse is not food on the tick it falls. Half the plant's twelve-unit
    // reproduction threshold stands in for its body mass.
    REQUIRE(simulation.world().tileAt(1, 1).detritusLevel == Catch::Approx(6.0));
    REQUIRE(livingOrganisms(simulation.world()).front()->remainingEnergyUnits ==
            Catch::Approx(5.0).margin(kSeasonalDriftMargin));

    const double soilBeforeDecomposition =
        simulation.world().tileAt(1, 1).soilNutrientLevel;
    simulation.advanceOneTick();
    // Five units of appetite against six of detritus: the decomposer eats its
    // fill, one unit is left in the ground, and part of what it processed is
    // mineralized into the soil it is standing on.
    REQUIRE(simulation.world().tileAt(1, 1).detritusLevel ==
            Catch::Approx(1.0).margin(kSeasonalDriftMargin));
    REQUIRE(livingOrganisms(simulation.world()).front()->remainingEnergyUnits ==
            Catch::Approx(9.0).margin(kSeasonalDriftMargin));
    REQUIRE(simulation.world().tileAt(1, 1).soilNutrientLevel > soilBeforeDecomposition);
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

TEST_CASE("photosynthesis scales with the soil a plant stands in",
          "[simulation][nutrients]") {
    const Species species = makePlant();

    const auto energyAfterOneTickOnSoil = [&species](double soilNutrientLevel) {
        WorldGrid world = makeUniformWorld(1, 1, kIdealTemperatureCelsius,
                                           kIdealHumidityPercent, soilNutrientLevel);
        Organism& plant = placeOrganism(world, species, 0, 0);
        plant.remainingEnergyUnits = 6.0;
        RandomNumberGenerator random(1);
        SimulationEngine simulation(std::move(world), random);
        simulation.advanceOneTick();
        const std::vector<const Organism*> living = livingOrganisms(simulation.world());
        return living.empty() ? 0.0 : living.front()->remainingEnergyUnits;
    };

    // Six to start, one of upkeep, then a four-unit yield scaled by the soil.
    REQUIRE(energyAfterOneTickOnSoil(1.0) ==
            Catch::Approx(9.0).margin(kSeasonalDriftMargin));
    // Half the level that saturates photosynthesis, so half the yield.
    REQUIRE(energyAfterOneTickOnSoil(0.25) ==
            Catch::Approx(7.0).margin(kSeasonalDriftMargin));
    // Dead soil grows nothing, so the plant only pays upkeep.
    REQUIRE(energyAfterOneTickOnSoil(0.0) ==
            Catch::Approx(5.0).margin(kSeasonalDriftMargin));
}

TEST_CASE("a sealed plot of plants exhausts its soil and stops supporting them",
          "[simulation][nutrients]") {
    const Species species = makePlant();
    WorldGrid world = makeUniformWorld(1, 1);
    placeOrganism(world, species, 0, 0);

    RandomNumberGenerator random(4);
    SimulationEngine simulation(std::move(world), random);

    const double startingSoil = simulation.world().tileAt(0, 0).soilNutrientLevel;
    for (int tick = 0; tick < 400; ++tick) {
        simulation.advanceOneTick();
    }

    // Nothing returns matter to this tile but the plant's own death, and the plant
    // draws on it every tick it grows, so fertility only ever falls.
    REQUIRE(simulation.world().tileAt(0, 0).soilNutrientLevel < startingSoil);
    REQUIRE(simulation.latestTickReport().livingCountOf(OrganismCategory::Plant) == 0);
}

TEST_CASE("decomposers recover a plot that has run its soil down",
          "[simulation][nutrients]") {
    const Species plantSpecies = makePlant();
    const Species decomposerSpecies = makeDecomposer();

    /// What a sealed plot looks like after the run: whether its plants are still
    /// alive, and how much of its matter sits in soil rather than in detritus.
    struct PlotOutcome {
        int livingPlantCount = 0;
        double totalSoilNutrientLevel = 0.0;
        double totalDetritusLevel = 0.0;
    };

    /// Runs a plot of exhausted soil piled with dead matter, with decomposers
    /// seeded or not. Both arms share a seed and are identical but for the
    /// decomposers, so any difference between them is the decomposers' work.
    const auto runPlot = [&](bool withDecomposers) {
        constexpr int kPlotSizeInTiles = 4;
        // Soil far below the level that saturates photosynthesis, so the plants
        // cannot cover their own upkeep on it and need the cycle to restart.
        constexpr double kExhaustedSoilNutrientLevel = 0.05;
        // Dead matter left from whatever lived here before, enough to rebuild the
        // soil if something works it and inert if nothing does.
        constexpr double kStartingDetritusPerTile = 15.0;
        constexpr double kStartingEnergyUnits = 10.0;

        WorldGrid world =
            makeUniformWorld(kPlotSizeInTiles, kPlotSizeInTiles, kIdealTemperatureCelsius,
                             kIdealHumidityPercent, kExhaustedSoilNutrientLevel);
        world.forEachTile([](int, int, petriterm::world::Tile& tile) {
            tile.detritusLevel = kStartingDetritusPerTile;
        });
        for (int columnIndex = 0; columnIndex < kPlotSizeInTiles; ++columnIndex) {
            for (int rowIndex = 0; rowIndex < kPlotSizeInTiles; ++rowIndex) {
                placeOrganism(world, plantSpecies, columnIndex, rowIndex)
                    .remainingEnergyUnits = kStartingEnergyUnits;
                if (withDecomposers) {
                    placeOrganism(world, decomposerSpecies, columnIndex, rowIndex)
                        .remainingEnergyUnits = kStartingEnergyUnits;
                }
            }
        }

        RandomNumberGenerator random(11);
        SimulationEngine simulation(std::move(world), random);
        // Forty ticks sits inside the opening stretch of Clear weather, for the
        // same reason the photosynthesis test above stops at fifty: past it a
        // cold snap kills every plant on the plot regardless of the soil, which
        // would mask the thing being measured.
        for (int tick = 0; tick < 40; ++tick) {
            simulation.advanceOneTick();
        }

        PlotOutcome outcome;
        outcome.livingPlantCount =
            simulation.latestTickReport().livingCountOf(OrganismCategory::Plant);
        simulation.world().forEachTile(
            [&outcome](int, int, const petriterm::world::Tile& tile) {
                outcome.totalSoilNutrientLevel += tile.soilNutrientLevel;
                outcome.totalDetritusLevel += tile.detritusLevel;
            });
        return outcome;
    };

    const PlotOutcome withoutDecomposers = runPlot(false);
    const PlotOutcome withDecomposers = runPlot(true);

    // Nothing works the detritus, so it just accumulates as the plants starve on
    // soil none of it ever reaches.
    REQUIRE(withoutDecomposers.livingPlantCount == 0);
    REQUIRE(withoutDecomposers.totalDetritusLevel > 15.0 * 4 * 4);

    // With decomposers the same matter is mineralized into the soil instead, and
    // the plants that would otherwise have starved are all still standing.
    REQUIRE(withDecomposers.livingPlantCount == 4 * 4);
    REQUIRE(withDecomposers.totalSoilNutrientLevel >
            10.0 * withoutDecomposers.totalSoilNutrientLevel);
    REQUIRE(withDecomposers.totalDetritusLevel < withoutDecomposers.totalDetritusLevel);
}

TEST_CASE("detritus on one tile is capped", "[simulation][nutrients]") {
    Species species = makePlant();
    // A threshold high enough that a handful of corpses would blow past any
    // plausible cap on their own.
    species.traits.energyRequiredToReproduce = 400.0;
    WorldGrid world = makeUniformWorld(1, 1);
    for (int index = 0; index < 4; ++index) {
        Organism& dying = placeOrganism(world, species, 0, 0);
        dying.remainingEnergyUnits = 1.0;
        // Dead soil, so nothing photosynthesizes its way out of starving.
        dying.ticksUntilCanReproduce = 0;
    }
    world.tileAt(0, 0).soilNutrientLevel = 0.0;

    RandomNumberGenerator random(1);
    SimulationEngine simulation(std::move(world), random);
    simulation.advanceOneTick();

    REQUIRE(simulation.latestTickReport().deathCount == 4);
    REQUIRE(simulation.world().tileAt(0, 0).detritusLevel <= 100.0);
    REQUIRE(simulation.world().tileAt(0, 0).detritusLevel == Catch::Approx(100.0));
}
