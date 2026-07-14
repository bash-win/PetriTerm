#pragma once

#include <functional>

#include "petriterm/engine/Grid.hpp"
#include "petriterm/world/Tile.hpp"

namespace petriterm::world {

/// Identifies one tile position in the world.
struct TileCoordinate {
    int columnIndex = 0;
    int rowIndex = 0;
};

/// Owns the rectangular grid of tiles making up the world and provides
/// world-oriented access on top of the generic engine grid. Constructed empty;
/// tiles are populated by WorldGenerator.
class WorldGrid {
public:
    /// Constructs a world of the given tile dimensions with default-valued
    /// tiles. Precondition: both dimensions are positive.
    WorldGrid(int widthInTiles, int heightInTiles);

    int widthInTiles() const;
    int heightInTiles() const;

    /// Returns the tile at the given coordinate. Precondition: in bounds.
    Tile& tileAt(int columnIndex, int rowIndex);

    /// Returns the tile at the given coordinate for read-only access.
    /// Precondition: in bounds.
    const Tile& tileAt(int columnIndex, int rowIndex) const;

    /// Returns true if the coordinate lies within the world.
    bool containsCoordinate(int columnIndex, int rowIndex) const;

    /// Applies the callback to every tile exactly once in row-major order,
    /// passing the tile's column, row, and a mutable reference. Used by the
    /// climate and simulation passes.
    void forEachTile(const std::function<void(int, int, Tile&)>& callback);

    /// Applies the callback to every tile exactly once in row-major order with
    /// read-only access. Used by rendering and metrics.
    void forEachTile(const std::function<void(int, int, const Tile&)>& callback) const;

private:
    engine::Grid<Tile> tiles;
};

}
