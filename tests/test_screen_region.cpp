#include <catch2/catch_test_macros.hpp>

#include "petriterm/engine/ScreenRegion.hpp"

using petriterm::engine::ScreenRegion;

TEST_CASE("a default-constructed screen region is empty", "[screen-region]") {
    constexpr ScreenRegion region;
    REQUIRE(region.isEmpty());
}

TEST_CASE("a screen region reports its far edges", "[screen-region]") {
    constexpr ScreenRegion region{4, 2, 10, 5};
    REQUIRE_FALSE(region.isEmpty());
    REQUIRE(region.rightColumn() == 13);
    REQUIRE(region.bottomRow() == 6);
}

TEST_CASE("a zero extent along either axis is empty", "[screen-region]") {
    REQUIRE(ScreenRegion{0, 0, 0, 24}.isEmpty());
    REQUIRE(ScreenRegion{0, 0, 80, 0}.isEmpty());
    REQUIRE_FALSE(ScreenRegion{0, 0, 1, 1}.isEmpty());
}

TEST_CASE("withoutBottomRows reserves rows along the bottom edge", "[screen-region]") {
    constexpr ScreenRegion screen{0, 0, 80, 24};
    constexpr ScreenRegion mapPane = screen.withoutBottomRows(1);
    REQUIRE(mapPane.leftColumn == 0);
    REQUIRE(mapPane.topRow == 0);
    REQUIRE(mapPane.widthInColumns == 80);
    REQUIRE(mapPane.heightInRows == 23);
    // The reserved row is the one the original region ended on.
    REQUIRE(mapPane.bottomRow() + 1 == screen.bottomRow());
}

TEST_CASE("withoutBottomRows clamps at empty instead of inverting", "[screen-region]") {
    constexpr ScreenRegion oneRow{0, 0, 80, 1};
    REQUIRE(oneRow.withoutBottomRows(1).isEmpty());
    REQUIRE(oneRow.withoutBottomRows(1).heightInRows == 0);

    // Reserving more rows than exist is what a terminal shrunk below the bar
    // height produces, and it has to yield nothing to draw rather than a
    // negative height that later arithmetic would read as a huge one.
    REQUIRE(oneRow.withoutBottomRows(50).heightInRows == 0);
    REQUIRE(ScreenRegion{}.withoutBottomRows(3).heightInRows == 0);
}

TEST_CASE("withoutBottomRows ignores a negative row count", "[screen-region]") {
    constexpr ScreenRegion screen{0, 0, 80, 24};
    REQUIRE(screen.withoutBottomRows(-5).heightInRows == 24);
}

TEST_CASE("screen regions compare by every field", "[screen-region]") {
    constexpr ScreenRegion region{1, 2, 3, 4};
    REQUIRE(region == ScreenRegion{1, 2, 3, 4});
    REQUIRE(region != ScreenRegion{0, 2, 3, 4});
    REQUIRE(region != ScreenRegion{1, 0, 3, 4});
    REQUIRE(region != ScreenRegion{1, 2, 0, 4});
    REQUIRE(region != ScreenRegion{1, 2, 3, 0});
}
