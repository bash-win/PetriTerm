#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <vector>

namespace petriterm::engine {

/// Selects which surrounding cells count as neighbors during grid traversal:
/// FourWay is the orthogonal (edge-sharing) neighborhood, EightWay adds the
/// four diagonal (corner-sharing) cells.
enum class NeighborAdjacency {
    FourWay,
    EightWay,
};

/// Generic row-major 2D container with bounds-checked access and neighbor
/// iteration. Ecology-agnostic; every cell is value-initialized on construction.
template <typename T>
class Grid {
public:
    /// Constructs a grid of the given dimensions, value-initializing every cell.
    /// Precondition: both dimensions are positive.
    Grid(int widthInColumns, int heightInRows)
        : columnCount(widthInColumns),
          rowCount(heightInRows),
          cells(static_cast<std::size_t>(widthInColumns) *
                static_cast<std::size_t>(heightInRows)) {
        assert(widthInColumns > 0 && heightInRows > 0);
    }

    int widthInColumns() const { return columnCount; }
    int heightInRows() const { return rowCount; }

    /// Returns a mutable reference to the cell at the given coordinate.
    /// Precondition: the coordinate is in bounds.
    T& cellAt(int columnIndex, int rowIndex) {
        assert(containsCoordinate(columnIndex, rowIndex));
        return cells[cellIndexFor(columnIndex, rowIndex)];
    }

    /// Returns an immutable reference to the cell at the given coordinate.
    /// Precondition: the coordinate is in bounds.
    const T& cellAt(int columnIndex, int rowIndex) const {
        assert(containsCoordinate(columnIndex, rowIndex));
        return cells[cellIndexFor(columnIndex, rowIndex)];
    }

    /// Returns true if the coordinate lies within the grid.
    bool containsCoordinate(int columnIndex, int rowIndex) const {
        return columnIndex >= 0 && rowIndex >= 0 && columnIndex < columnCount &&
               rowIndex < rowCount;
    }

    /// Invokes the callback with the coordinates of each of the (up to eight)
    /// in-bounds neighbors of the given cell, respecting the requested adjacency.
    /// The center cell itself is never visited.
    void forEachNeighbor(int columnIndex, int rowIndex, NeighborAdjacency adjacency,
                         const std::function<void(int, int)>& callback) const {
        for (int rowOffset = -1; rowOffset <= 1; ++rowOffset) {
            for (int columnOffset = -1; columnOffset <= 1; ++columnOffset) {
                if (rowOffset == 0 && columnOffset == 0) {
                    continue;
                }
                const bool isDiagonal = rowOffset != 0 && columnOffset != 0;
                if (adjacency == NeighborAdjacency::FourWay && isDiagonal) {
                    continue;
                }
                const int neighborColumn = columnIndex + columnOffset;
                const int neighborRow = rowIndex + rowOffset;
                if (containsCoordinate(neighborColumn, neighborRow)) {
                    callback(neighborColumn, neighborRow);
                }
            }
        }
    }

private:
    std::size_t cellIndexFor(int columnIndex, int rowIndex) const {
        return static_cast<std::size_t>(rowIndex) * static_cast<std::size_t>(columnCount) +
               static_cast<std::size_t>(columnIndex);
    }

    int columnCount;
    int rowCount;
    std::vector<T> cells;
};

}
