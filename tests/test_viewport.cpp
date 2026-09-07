#include <catch2/catch_test_macros.hpp>

#include "petriterm/engine/ScreenRegion.hpp"
#include "petriterm/game/Viewport.hpp"

using petriterm::engine::ScreenRegion;
using petriterm::game::ScreenCell;
using petriterm::game::Viewport;

TEST_CASE("Viewport starts with the camera at the world origin", "[viewport]") {
    const Viewport viewport(100, 100, ScreenRegion{0, 0, 10, 10});
    REQUIRE(viewport.cameraColumnIndex() == 0);
    REQUIRE(viewport.cameraRowIndex() == 0);
    REQUIRE(viewport.visibleWidthInTiles() == 10);
    REQUIRE(viewport.visibleHeightInTiles() == 10);
}

TEST_CASE("scrollByTiles moves and clamps the camera to world bounds", "[viewport]") {
    Viewport viewport(100, 80, ScreenRegion{0, 0, 10, 10});

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
    Viewport viewport(5, 5, ScreenRegion{0, 0, 10, 10});
    viewport.scrollByTiles(3, 3);
    REQUIRE(viewport.cameraColumnIndex() == 0);
    REQUIRE(viewport.cameraRowIndex() == 0);
    REQUIRE(viewport.visibleWidthInTiles() == 5);
    REQUIRE(viewport.visibleHeightInTiles() == 5);
}

TEST_CASE("tileToScreenCell maps visible tiles and rejects off-screen ones", "[viewport]") {
    const Viewport viewport(100, 100, ScreenRegion{20, 2, 10, 10});

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
    Viewport viewport(100, 100, ScreenRegion{0, 0, 10, 10});
    viewport.scrollByTiles(5, 5);

    const auto cornerCell = viewport.tileToScreenCell(5, 5);
    REQUIRE(cornerCell.has_value());
    REQUIRE(cornerCell->columnIndex == 0);
    REQUIRE(cornerCell->rowIndex == 0);

    REQUIRE_FALSE(viewport.tileToScreenCell(4, 4).has_value());
}

TEST_CASE("ensureTileVisible scrolls the minimum needed in each direction", "[viewport]") {
    Viewport viewport(100, 100, ScreenRegion{0, 0, 10, 10});

    viewport.ensureTileVisible(15, 0);
    REQUIRE(viewport.cameraColumnIndex() == 6);
    REQUIRE(viewport.tileToScreenCell(15, 0).has_value());

    viewport.ensureTileVisible(3, 0);
    REQUIRE(viewport.cameraColumnIndex() == 3);

    viewport.ensureTileVisible(50, 40);
    REQUIRE(viewport.tileToScreenCell(50, 40).has_value());
}

TEST_CASE("setScreenRegion moves where the same tiles are drawn", "[viewport]") {
    Viewport viewport(100, 100, ScreenRegion{0, 0, 10, 10});
    viewport.setScreenRegion(ScreenRegion{20, 2, 10, 10});

    const auto originCell = viewport.tileToScreenCell(0, 0);
    REQUIRE(originCell.has_value());
    REQUIRE(originCell->columnIndex == 20);
    REQUIRE(originCell->rowIndex == 2);
    REQUIRE(viewport.cameraColumnIndex() == 0);
    REQUIRE(viewport.cameraRowIndex() == 0);
}

TEST_CASE("growing the screen region pulls the camera back inside the world",
          "[viewport]") {
    Viewport viewport(100, 100, ScreenRegion{0, 0, 10, 10});
    viewport.scrollByTiles(1000, 1000);
    REQUIRE(viewport.cameraColumnIndex() == 90);
    REQUIRE(viewport.cameraRowIndex() == 90);

    // The camera was parked at the world's bottom-right corner, so a wider view
    // has to scroll back rather than show tiles past the edge.
    viewport.setScreenRegion(ScreenRegion{0, 0, 40, 25});
    REQUIRE(viewport.cameraColumnIndex() == 60);
    REQUIRE(viewport.cameraRowIndex() == 75);
    REQUIRE(viewport.visibleWidthInTiles() == 40);
    REQUIRE(viewport.visibleHeightInTiles() == 25);
    REQUIRE(viewport.tileToScreenCell(99, 99).has_value());
}

TEST_CASE("shrinking the screen region leaves the top-left tile where it was",
          "[viewport]") {
    Viewport viewport(100, 100, ScreenRegion{0, 0, 40, 25});
    viewport.scrollByTiles(12, 7);

    viewport.setScreenRegion(ScreenRegion{0, 0, 10, 10});
    REQUIRE(viewport.cameraColumnIndex() == 12);
    REQUIRE(viewport.cameraRowIndex() == 7);
    REQUIRE(viewport.visibleWidthInTiles() == 10);
    REQUIRE(viewport.visibleHeightInTiles() == 10);
    REQUIRE_FALSE(viewport.tileToScreenCell(30, 20).has_value());
}

TEST_CASE("a screen region larger than the world caps the visible tile count",
          "[viewport]") {
    Viewport viewport(20, 15, ScreenRegion{0, 0, 10, 10});
    viewport.setScreenRegion(ScreenRegion{0, 0, 200, 100});
    REQUIRE(viewport.visibleWidthInTiles() == 20);
    REQUIRE(viewport.visibleHeightInTiles() == 15);
    REQUIRE(viewport.cameraColumnIndex() == 0);
    REQUIRE(viewport.cameraRowIndex() == 0);
}

TEST_CASE("an empty screen region shows nothing and keeps the camera in the world",
          "[viewport]") {
    Viewport viewport(100, 100, ScreenRegion{0, 0, 10, 10});
    viewport.scrollByTiles(40, 40);

    // A terminal short enough that the help bar leaves no rows for the map. The
    // camera must stay inside the world through it, because the region grows back
    // and whatever position survived is the one the player returns to.
    viewport.setScreenRegion(ScreenRegion{0, 0, 0, 0});
    REQUIRE(viewport.visibleWidthInTiles() == 0);
    REQUIRE(viewport.visibleHeightInTiles() == 0);
    REQUIRE_FALSE(viewport.tileToScreenCell(40, 40).has_value());
    REQUIRE(viewport.cameraColumnIndex() < 100);
    REQUIRE(viewport.cameraRowIndex() < 100);

    viewport.setScreenRegion(ScreenRegion{0, 0, 10, 10});
    REQUIRE(viewport.cameraColumnIndex() == 40);
    REQUIRE(viewport.cameraRowIndex() == 40);
    REQUIRE(viewport.tileToScreenCell(40, 40).has_value());
}
