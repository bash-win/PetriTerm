#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "petriterm/organisms/Organism.hpp"
#include "petriterm/organisms/Species.hpp"
#include "petriterm/world/Tile.hpp"

using petriterm::organisms::Organism;
using petriterm::organisms::OrganismCategory;
using petriterm::organisms::Species;
using petriterm::world::Tile;

namespace {

Species makeSpecies(OrganismCategory category) {
    Species species;
    species.category = category;
    species.traits.energyRequiredToReproduce = 10.0;
    return species;
}

}

TEST_CASE("a fresh tile holds no organisms", "[tile]") {
    const Tile tile;
    REQUIRE(tile.livingOrganismCount() == 0);
    REQUIRE(tile.hasCapacityForCategory(OrganismCategory::Plant));
}

TEST_CASE("livingOrganismCount ignores dead organisms", "[tile]") {
    const Species plant = makeSpecies(OrganismCategory::Plant);
    Tile tile;
    tile.occupyingOrganisms.push_back(std::make_unique<Organism>(&plant, 0, 0));
    tile.occupyingOrganisms.push_back(std::make_unique<Organism>(&plant, 0, 0));
    tile.occupyingOrganisms.back()->isAlive = false;
    REQUIRE(tile.livingOrganismCount() == 1);
}

TEST_CASE("hasCapacityForCategory enforces the per-category limit", "[tile]") {
    const Species carnivore = makeSpecies(OrganismCategory::Carnivore);
    Tile tile;
    REQUIRE(tile.hasCapacityForCategory(OrganismCategory::Carnivore));
    tile.occupyingOrganisms.push_back(std::make_unique<Organism>(&carnivore, 0, 0));
    REQUIRE_FALSE(tile.hasCapacityForCategory(OrganismCategory::Carnivore));
}

TEST_CASE("capacity is tracked per category independently", "[tile]") {
    const Species carnivore = makeSpecies(OrganismCategory::Carnivore);
    const Species plant = makeSpecies(OrganismCategory::Plant);
    Tile tile;
    tile.occupyingOrganisms.push_back(std::make_unique<Organism>(&carnivore, 0, 0));
    REQUIRE_FALSE(tile.hasCapacityForCategory(OrganismCategory::Carnivore));
    REQUIRE(tile.hasCapacityForCategory(OrganismCategory::Plant));
}

TEST_CASE("a dead organism frees its capacity slot", "[tile]") {
    const Species carnivore = makeSpecies(OrganismCategory::Carnivore);
    Tile tile;
    tile.occupyingOrganisms.push_back(std::make_unique<Organism>(&carnivore, 0, 0));
    REQUIRE_FALSE(tile.hasCapacityForCategory(OrganismCategory::Carnivore));
    tile.occupyingOrganisms.back()->isAlive = false;
    REQUIRE(tile.hasCapacityForCategory(OrganismCategory::Carnivore));
}
