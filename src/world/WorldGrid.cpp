#include "petriterm/world/WorldGrid.hpp"

namespace petriterm::world {

WorldGrid::WorldGrid(int widthInTiles, int heightInTiles)
    : tiles(widthInTiles, heightInTiles) {}

int WorldGrid::widthInTiles() const {
    return tiles.widthInColumns();
}

int WorldGrid::heightInTiles() const {
    return tiles.heightInRows();
}

Tile& WorldGrid::tileAt(int columnIndex, int rowIndex) {
    return tiles.cellAt(columnIndex, rowIndex);
}

const Tile& WorldGrid::tileAt(int columnIndex, int rowIndex) const {
    return tiles.cellAt(columnIndex, rowIndex);
}

bool WorldGrid::containsCoordinate(int columnIndex, int rowIndex) const {
    return tiles.containsCoordinate(columnIndex, rowIndex);
}

void WorldGrid::forEachTile(const std::function<void(int, int, Tile&)>& callback) {
    for (int rowIndex = 0; rowIndex < tiles.heightInRows(); ++rowIndex) {
        for (int columnIndex = 0; columnIndex < tiles.widthInColumns(); ++columnIndex) {
            callback(columnIndex, rowIndex, tiles.cellAt(columnIndex, rowIndex));
        }
    }
}

void WorldGrid::forEachTile(
    const std::function<void(int, int, const Tile&)>& callback) const {
    for (int rowIndex = 0; rowIndex < tiles.heightInRows(); ++rowIndex) {
        for (int columnIndex = 0; columnIndex < tiles.widthInColumns(); ++columnIndex) {
            callback(columnIndex, rowIndex, tiles.cellAt(columnIndex, rowIndex));
        }
    }
}

}
