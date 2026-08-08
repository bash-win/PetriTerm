#include <catch2/catch_test_macros.hpp>

#include "petriterm/organisms/Organism.hpp"
#include "petriterm/organisms/Species.hpp"

using petriterm::organisms::Organism;
using petriterm::organisms::OrganismCategory;
using petriterm::organisms::Species;

namespace {

Species makeHerbivore() {
    Species species;
    species.speciesId = "test_hare";
    species.category = OrganismCategory::Herbivore;
    species.traits.energyConsumedPerTick = 2.0;
    species.traits.energyRequiredToReproduce = 20.0;
    species.traits.reproductionCooldownTicks = 3;
    species.traits.maximumAgeInTicks = 100;
    return species;
}

}

TEST_CASE("a new organism starts alive with partial energy and a cooldown", "[organism]") {
    const Species species = makeHerbivore();
    const Organism organism(&species, 4, 7);
    REQUIRE(organism.isAlive);
    REQUIRE(organism.tileColumnIndex == 4);
    REQUIRE(organism.tileRowIndex == 7);
    REQUIRE(organism.ageInTicks == 0);
    REQUIRE(organism.remainingEnergyUnits == 10.0);
    REQUIRE(organism.ticksUntilCanReproduce == 3);
    REQUIRE_FALSE(organism.isReadyToReproduce());
}

TEST_CASE("metabolism drains energy, ages, and counts down the cooldown", "[organism]") {
    const Species species = makeHerbivore();
    Organism organism(&species, 0, 0);
    organism.applyMetabolismAndAgingForOneTick();
    REQUIRE(organism.remainingEnergyUnits == 8.0);
    REQUIRE(organism.ageInTicks == 1);
    REQUIRE(organism.ticksUntilCanReproduce == 2);
    REQUIRE(organism.isAlive);
}

TEST_CASE("an organism dies when its energy is depleted", "[organism]") {
    const Species species = makeHerbivore();
    Organism organism(&species, 0, 0);
    for (int tick = 0; tick < 5; ++tick) {
        organism.applyMetabolismAndAgingForOneTick();
    }
    REQUIRE_FALSE(organism.isAlive);
}

TEST_CASE("an organism dies once it exceeds its maximum age", "[organism]") {
    Species species = makeHerbivore();
    species.traits.energyConsumedPerTick = 0.0;
    species.traits.maximumAgeInTicks = 3;
    Organism organism(&species, 0, 0);
    for (int tick = 0; tick < 3; ++tick) {
        organism.applyMetabolismAndAgingForOneTick();
    }
    REQUIRE(organism.isAlive);
    organism.applyMetabolismAndAgingForOneTick();
    REQUIRE_FALSE(organism.isAlive);
}

TEST_CASE("isReadyToReproduce requires both energy and no cooldown", "[organism]") {
    const Species species = makeHerbivore();
    Organism organism(&species, 0, 0);
    organism.remainingEnergyUnits = 25.0;
    REQUIRE_FALSE(organism.isReadyToReproduce());

    organism.ticksUntilCanReproduce = 0;
    REQUIRE(organism.isReadyToReproduce());

    organism.remainingEnergyUnits = 5.0;
    REQUIRE_FALSE(organism.isReadyToReproduce());
}
