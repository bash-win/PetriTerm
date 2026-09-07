#include "petriterm/game/Viewport.hpp"

#include <algorithm>

namespace petriterm::game {

Viewport::Viewport(int worldWidthInTiles, int worldHeightInTiles,
                   const engine::ScreenRegion& screenRegion)
    : worldWidthInTiles(worldWidthInTiles),
      worldHeightInTiles(worldHeightInTiles),
      screenRegion(screenRegion) {}

void Viewport::setScreenRegion(const engine::ScreenRegion& newScreenRegion) {
    screenRegion = newScreenRegion;
    clampCameraToWorldBounds();
}

int Viewport::scrollSpanInColumns() const {
    // An empty region reports nothing visible but still needs a span of at least
    // one for the camera arithmetic, or the maximum camera position becomes the
    // whole world width and the camera can be parked outside the world - which
    // would then persist after the region grows back.
    return std::max(1, screenRegion.widthInColumns);
}

int Viewport::scrollSpanInRows() const {
    return std::max(1, screenRegion.heightInRows);
}

int Viewport::maximumCameraColumn() const {
    return std::max(0, worldWidthInTiles - scrollSpanInColumns());
}

int Viewport::maximumCameraRow() const {
    return std::max(0, worldHeightInTiles - scrollSpanInRows());
}

void Viewport::clampCameraToWorldBounds() {
    cameraColumn = std::clamp(cameraColumn, 0, maximumCameraColumn());
    cameraRow = std::clamp(cameraRow, 0, maximumCameraRow());
}

int Viewport::visibleWidthInTiles() const {
    return std::clamp(screenRegion.widthInColumns, 0, worldWidthInTiles);
}

int Viewport::visibleHeightInTiles() const {
    return std::clamp(screenRegion.heightInRows, 0, worldHeightInTiles);
}

void Viewport::scrollByTiles(int columnDelta, int rowDelta) {
    cameraColumn += columnDelta;
    cameraRow += rowDelta;
    clampCameraToWorldBounds();
}

void Viewport::ensureTileVisible(int columnIndex, int rowIndex) {
    const int spanInColumns = scrollSpanInColumns();
    const int spanInRows = scrollSpanInRows();
    if (columnIndex < cameraColumn) {
        cameraColumn = columnIndex;
    } else if (columnIndex > cameraColumn + spanInColumns - 1) {
        cameraColumn = columnIndex - spanInColumns + 1;
    }
    if (rowIndex < cameraRow) {
        cameraRow = rowIndex;
    } else if (rowIndex > cameraRow + spanInRows - 1) {
        cameraRow = rowIndex - spanInRows + 1;
    }
    clampCameraToWorldBounds();
}

std::optional<ScreenCell> Viewport::tileToScreenCell(int columnIndex, int rowIndex) const {
    if (columnIndex < 0 || columnIndex >= worldWidthInTiles || rowIndex < 0 ||
        rowIndex >= worldHeightInTiles) {
        return std::nullopt;
    }
    const int visibleColumn = columnIndex - cameraColumn;
    const int visibleRow = rowIndex - cameraRow;
    if (visibleColumn < 0 || visibleColumn >= screenRegion.widthInColumns ||
        visibleRow < 0 || visibleRow >= screenRegion.heightInRows) {
        return std::nullopt;
    }
    return ScreenCell{screenRegion.leftColumn + visibleColumn,
                      screenRegion.topRow + visibleRow};
}

}
