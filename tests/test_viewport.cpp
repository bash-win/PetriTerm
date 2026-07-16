#include <catch2/catch_test_macros.hpp>

#include "petriterm/game/Viewport.hpp"

using petriterm::game::ScreenCell;
using petriterm::game::Viewport;

TEST_CASE("Viewport starts with the camera at the world origin", "[viewport]") {
    const Viewport viewport(100, 100, 0, 0, 10, 10);
    REQUIRE(viewport.cameraColumnIndex() == 0);
    REQUIRE(viewport.cameraRowIndex() == 0);
    REQUIRE(viewport.visibleWidthInTiles() == 10);
    REQUIRE(viewport.visibleHeightInTiles() == 10);
}

TEST_CASE("scrollByTiles moves and clamps the camera to world bounds", "[viewport]") {
    Viewport viewport(100, 80, 0, 0, 10, 10);

    viewport.scrollByTiles(5, 3);
    REQUIRE(viewport.cameraColumnIndex() == 5);
    REQUIRE(viewport.cameraRowIndex() == 3);

    viewport.scrollByTiles(1000, 1000);
    REQUIRE(viewport.cameraColumnIndex() == 90);
    REQUIRE(viewport.cameraRowIndex() == 70);

    viewport.scrollByTiles(-1000, -1000);
    REQUIRE(viewport.cameraColumnIndex() == 0);
    REQUIRE(viewport.cameraRowIndex() == 0);
}

TEST_CASE("a world smaller than the screen never scrolls", "[viewport]") {
    Viewport viewport(5, 5, 0, 0, 10, 10);
    viewport.scrollByTiles(3, 3);
    REQUIRE(viewport.cameraColumnIndex() == 0);
    REQUIRE(viewport.cameraRowIndex() == 0);
    REQUIRE(viewport.visibleWidthInTiles() == 5);
    REQUIRE(viewport.visibleHeightInTiles() == 5);
}

TEST_CASE("tileToScreenCell maps visible tiles and rejects off-screen ones", "[viewport]") {
    const Viewport viewport(100, 100, 20, 2, 10, 10);

    const auto originCell = viewport.tileToScreenCell(0, 0);
    REQUIRE(originCell.has_value());
    REQUIRE(originCell->columnIndex == 20);
    REQUIRE(originCell->rowIndex == 2);

    const auto insideCell = viewport.tileToScreenCell(3, 4);
    REQUIRE(insideCell.has_value());
    REQUIRE(insideCell->columnIndex == 23);
    REQUIRE(insideCell->rowIndex == 6);

    REQUIRE_FALSE(viewport.tileToScreenCell(50, 50).has_value());
    REQUIRE_FALSE(viewport.tileToScreenCell(-1, 0).has_value());
    REQUIRE_FALSE(viewport.tileToScreenCell(100, 0).has_value());
}

TEST_CASE("tileToScreenCell follows the camera after scrolling", "[viewport]") {
    Viewport viewport(100, 100, 0, 0, 10, 10);
    viewport.scrollByTiles(5, 5);

    const auto cornerCell = viewport.tileToScreenCell(5, 5);
    REQUIRE(cornerCell.has_value());
    REQUIRE(cornerCell->columnIndex == 0);
    REQUIRE(cornerCell->rowIndex == 0);

    REQUIRE_FALSE(viewport.tileToScreenCell(4, 4).has_value());
}

TEST_CASE("ensureTileVisible scrolls the minimum needed in each direction", "[viewport]") {
    Viewport viewport(100, 100, 0, 0, 10, 10);

    viewport.ensureTileVisible(15, 0);
    REQUIRE(viewport.cameraColumnIndex() == 6);
    REQUIRE(viewport.tileToScreenCell(15, 0).has_value());

    viewport.ensureTileVisible(3, 0);
    REQUIRE(viewport.cameraColumnIndex() == 3);

    viewport.ensureTileVisible(50, 40);
    REQUIRE(viewport.tileToScreenCell(50, 40).has_value());
}
