#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

#include "petriterm/engine/Grid.hpp"

using petriterm::engine::Grid;
using petriterm::engine::NeighborAdjacency;

TEST_CASE("Grid reports its dimensions and value-initializes cells", "[grid]") {
    const Grid<int> grid(4, 3);
    REQUIRE(grid.widthInColumns() == 4);
    REQUIRE(grid.heightInRows() == 3);
    for (int rowIndex = 0; rowIndex < grid.heightInRows(); ++rowIndex) {
        for (int columnIndex = 0; columnIndex < grid.widthInColumns(); ++columnIndex) {
            REQUIRE(grid.cellAt(columnIndex, rowIndex) == 0);
        }
    }
}

TEST_CASE("Grid containsCoordinate respects bounds", "[grid]") {
    const Grid<int> grid(4, 3);
    REQUIRE(grid.containsCoordinate(0, 0));
    REQUIRE(grid.containsCoordinate(3, 2));
    REQUIRE_FALSE(grid.containsCoordinate(-1, 0));
    REQUIRE_FALSE(grid.containsCoordinate(0, -1));
    REQUIRE_FALSE(grid.containsCoordinate(4, 0));
    REQUIRE_FALSE(grid.containsCoordinate(0, 3));
}

TEST_CASE("Grid stores cells in row-major order without aliasing", "[grid]") {
    Grid<int> grid(3, 2);
    grid.cellAt(0, 0) = 10;
    grid.cellAt(2, 0) = 20;
    grid.cellAt(1, 1) = 30;
    REQUIRE(grid.cellAt(0, 0) == 10);
    REQUIRE(grid.cellAt(2, 0) == 20);
    REQUIRE(grid.cellAt(1, 1) == 30);
    REQUIRE(grid.cellAt(1, 0) == 0);
    REQUIRE(grid.cellAt(0, 1) == 0);
}

TEST_CASE("Grid forEachNeighbor honors adjacency and bounds", "[grid]") {
    const Grid<int> grid(5, 5);

    SECTION("four-way interior cell has four neighbors") {
        int neighborCount = 0;
        grid.forEachNeighbor(2, 2, NeighborAdjacency::FourWay,
                             [&](int, int) { ++neighborCount; });
        REQUIRE(neighborCount == 4);
    }
    SECTION("eight-way interior cell has eight neighbors") {
        int neighborCount = 0;
        grid.forEachNeighbor(2, 2, NeighborAdjacency::EightWay,
                             [&](int, int) { ++neighborCount; });
        REQUIRE(neighborCount == 8);
    }
    SECTION("four-way corner cell has two neighbors") {
        int neighborCount = 0;
        grid.forEachNeighbor(0, 0, NeighborAdjacency::FourWay,
                             [&](int, int) { ++neighborCount; });
        REQUIRE(neighborCount == 2);
    }
    SECTION("eight-way corner cell has three neighbors") {
        int neighborCount = 0;
        grid.forEachNeighbor(0, 0, NeighborAdjacency::EightWay,
                             [&](int, int) { ++neighborCount; });
        REQUIRE(neighborCount == 3);
    }
    SECTION("visited neighbors are in bounds and exclude the center") {
        std::vector<std::pair<int, int>> visited;
        grid.forEachNeighbor(2, 2, NeighborAdjacency::EightWay,
                             [&](int neighborColumn, int neighborRow) {
                                 visited.emplace_back(neighborColumn, neighborRow);
                             });
        REQUIRE(visited.size() == 8);
        for (const auto& [neighborColumn, neighborRow] : visited) {
            REQUIRE(grid.containsCoordinate(neighborColumn, neighborRow));
            REQUIRE_FALSE((neighborColumn == 2 && neighborRow == 2));
        }
    }
}
