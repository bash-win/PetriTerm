#pragma once

#include <vector>

namespace petriterm::organisms {
struct Species;
}

namespace petriterm::world {
class WorldGrid;
}

namespace petriterm::game {

/// Manages the placement cursor and the currently selected palette species, and
/// places organisms into the world. Cursor coordinates are always clamped to the
/// world bounds; the selected species wraps around the palette.
class PlacementController {
public:
    /// Constructs a controller over a world of the given tile dimensions with the
    /// given ordered species palette (typically SpeciesRegistry::allSpecies()).
    PlacementController(int worldWidthInTiles, int worldHeightInTiles,
                        std::vector<const organisms::Species*> palette);

    /// Moves the placement cursor by the given tile deltas, clamped to the world.
    void moveCursorByTiles(int columnDelta, int rowDelta);

    /// Cycles the selected palette species by the given signed direction,
    /// wrapping around the ends.
    void selectAdjacentSpeciesInPalette(int direction);

    int cursorColumnIndex() const { return cursorColumn; }
    int cursorRowIndex() const { return cursorRow; }

    /// Returns the currently selected species, or nullptr if the palette is empty.
    const organisms::Species* selectedSpecies() const;

    /// Attempts to place the selected species at the cursor, deducting its
    /// eco-credit cost from the given balance. Returns false and changes nothing
    /// if the palette is empty, the balance is insufficient, or the tile is at
    /// capacity for the species' category.
    bool placeSelectedSpeciesAtCursor(world::WorldGrid& world, int& ecoCreditBalance) const;

private:
    int worldWidthInTiles;
    int worldHeightInTiles;
    int cursorColumn = 0;
    int cursorRow = 0;
    std::vector<const organisms::Species*> palette;
    int selectedSpeciesIndex = 0;
};

}
