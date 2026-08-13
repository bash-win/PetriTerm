#include "petriterm/game/PlacementController.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "petriterm/organisms/Organism.hpp"
#include "petriterm/organisms/Species.hpp"
#include "petriterm/world/Tile.hpp"
#include "petriterm/world/WorldGrid.hpp"

namespace petriterm::game {

PlacementController::PlacementController(int worldWidthInTiles, int worldHeightInTiles,
                                         std::vector<const organisms::Species*> palette)
    : worldWidthInTiles(worldWidthInTiles),
      worldHeightInTiles(worldHeightInTiles),
      palette(std::move(palette)) {}

void PlacementController::moveCursorByTiles(int columnDelta, int rowDelta) {
    cursorColumn = std::clamp(cursorColumn + columnDelta, 0, worldWidthInTiles - 1);
    cursorRow = std::clamp(cursorRow + rowDelta, 0, worldHeightInTiles - 1);
}

void PlacementController::selectAdjacentSpeciesInPalette(int direction) {
    if (palette.empty()) {
        return;
    }
    const int paletteSize = static_cast<int>(palette.size());
    selectedSpeciesIndex = (selectedSpeciesIndex + direction) % paletteSize;
    if (selectedSpeciesIndex < 0) {
        selectedSpeciesIndex += paletteSize;
    }
}

const organisms::Species* PlacementController::selectedSpecies() const {
    if (palette.empty()) {
        return nullptr;
    }
    return palette[static_cast<std::size_t>(selectedSpeciesIndex)];
}

bool PlacementController::placeSelectedSpeciesAtCursor(world::WorldGrid& world,
                                                       int& ecoCreditBalance) const {
    const organisms::Species* species = selectedSpecies();
    if (species == nullptr || species->ecoCreditCostToPlace > ecoCreditBalance) {
        return false;
    }
    world::Tile& tile = world.tileAt(cursorColumn, cursorRow);
    if (!tile.hasCapacityForCategory(species->category)) {
        return false;
    }
    ecoCreditBalance -= species->ecoCreditCostToPlace;
    tile.occupyingOrganisms.push_back(
        std::make_unique<organisms::Organism>(species, cursorColumn, cursorRow));
    return true;
}

}
