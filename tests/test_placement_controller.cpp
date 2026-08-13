#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "petriterm/game/PlacementController.hpp"
#include "petriterm/organisms/Species.hpp"
#include "petriterm/world/WorldGrid.hpp"

using petriterm::game::PlacementController;
using petriterm::organisms::OrganismCategory;
using petriterm::organisms::Species;
using petriterm::world::WorldGrid;

namespace {

Species makeSpecies(OrganismCategory category, int cost) {
    Species species;
    species.category = category;
    species.ecoCreditCostToPlace = cost;
    species.traits.energyRequiredToReproduce = 10.0;
    return species;
}

}

TEST_CASE("the cursor moves and clamps to the world bounds", "[placement]") {
    const std::vector<const Species*> emptyPalette;
    PlacementController controller(5, 4, emptyPalette);
    controller.moveCursorByTiles(2, 1);
    REQUIRE(controller.cursorColumnIndex() == 2);
    REQUIRE(controller.cursorRowIndex() == 1);
    controller.moveCursorByTiles(100, 100);
    REQUIRE(controller.cursorColumnIndex() == 4);
    REQUIRE(controller.cursorRowIndex() == 3);
    controller.moveCursorByTiles(-100, -100);
    REQUIRE(controller.cursorColumnIndex() == 0);
    REQUIRE(controller.cursorRowIndex() == 0);
}

TEST_CASE("species selection cycles and wraps around the palette", "[placement]") {
    const Species first = makeSpecies(OrganismCategory::Plant, 2);
    const Species second = makeSpecies(OrganismCategory::Herbivore, 5);
    const Species third = makeSpecies(OrganismCategory::Carnivore, 9);
    const std::vector<const Species*> palette{&first, &second, &third};
    PlacementController controller(10, 10, palette);

    REQUIRE(controller.selectedSpecies() == &first);
    controller.selectAdjacentSpeciesInPalette(1);
    REQUIRE(controller.selectedSpecies() == &second);
    controller.selectAdjacentSpeciesInPalette(1);
    controller.selectAdjacentSpeciesInPalette(1);
    REQUIRE(controller.selectedSpecies() == &first);
    controller.selectAdjacentSpeciesInPalette(-1);
    REQUIRE(controller.selectedSpecies() == &third);
}

TEST_CASE("an empty palette selects nothing and places nothing", "[placement]") {
    const std::vector<const Species*> emptyPalette;
    PlacementController controller(10, 10, emptyPalette);
    WorldGrid world(10, 10);
    int credits = 100;
    REQUIRE(controller.selectedSpecies() == nullptr);
    REQUIRE_FALSE(controller.placeSelectedSpeciesAtCursor(world, credits));
    REQUIRE(credits == 100);
}

TEST_CASE("placement spends credits and adds an organism", "[placement]") {
    const Species plant = makeSpecies(OrganismCategory::Plant, 2);
    const std::vector<const Species*> palette{&plant};
    PlacementController controller(10, 10, palette);
    controller.moveCursorByTiles(3, 4);
    WorldGrid world(10, 10);
    int credits = 10;

    REQUIRE(controller.placeSelectedSpeciesAtCursor(world, credits));
    REQUIRE(credits == 8);
    REQUIRE(world.tileAt(3, 4).livingOrganismCount() == 1);
}

TEST_CASE("placement is refused when credits are insufficient", "[placement]") {
    const Species carnivore = makeSpecies(OrganismCategory::Carnivore, 9);
    const std::vector<const Species*> palette{&carnivore};
    PlacementController controller(10, 10, palette);
    WorldGrid world(10, 10);
    int credits = 5;

    REQUIRE_FALSE(controller.placeSelectedSpeciesAtCursor(world, credits));
    REQUIRE(credits == 5);
    REQUIRE(world.tileAt(0, 0).livingOrganismCount() == 0);
}

TEST_CASE("placement is refused when the tile is at category capacity", "[placement]") {
    const Species carnivore = makeSpecies(OrganismCategory::Carnivore, 1);
    const std::vector<const Species*> palette{&carnivore};
    PlacementController controller(10, 10, palette);
    WorldGrid world(10, 10);
    int credits = 100;

    REQUIRE(controller.placeSelectedSpeciesAtCursor(world, credits));
    REQUIRE_FALSE(controller.placeSelectedSpeciesAtCursor(world, credits));
    REQUIRE(credits == 99);
    REQUIRE(world.tileAt(0, 0).livingOrganismCount() == 1);
}
