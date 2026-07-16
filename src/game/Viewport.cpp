#include "petriterm/game/Viewport.hpp"

#include <algorithm>

namespace petriterm::game {

Viewport::Viewport(int worldWidthInTiles, int worldHeightInTiles, int screenLeftColumn,
                   int screenTopRow, int screenWidth, int screenHeight)
    : worldWidthInTiles(worldWidthInTiles),
      worldHeightInTiles(worldHeightInTiles),
      screenLeftColumn(screenLeftColumn),
      screenTopRow(screenTopRow),
      screenWidth(std::max(1, screenWidth)),
      screenHeight(std::max(1, screenHeight)) {}

int Viewport::maximumCameraColumn() const {
    return std::max(0, worldWidthInTiles - screenWidth);
}

int Viewport::maximumCameraRow() const {
    return std::max(0, worldHeightInTiles - screenHeight);
}

void Viewport::clampCameraToWorldBounds() {
    cameraColumn = std::clamp(cameraColumn, 0, maximumCameraColumn());
    cameraRow = std::clamp(cameraRow, 0, maximumCameraRow());
}

int Viewport::visibleWidthInTiles() const {
    return std::min(screenWidth, worldWidthInTiles);
}

int Viewport::visibleHeightInTiles() const {
    return std::min(screenHeight, worldHeightInTiles);
}

void Viewport::scrollByTiles(int columnDelta, int rowDelta) {
    cameraColumn += columnDelta;
    cameraRow += rowDelta;
    clampCameraToWorldBounds();
}

void Viewport::ensureTileVisible(int columnIndex, int rowIndex) {
    if (columnIndex < cameraColumn) {
        cameraColumn = columnIndex;
    } else if (columnIndex > cameraColumn + screenWidth - 1) {
        cameraColumn = columnIndex - screenWidth + 1;
    }
    if (rowIndex < cameraRow) {
        cameraRow = rowIndex;
    } else if (rowIndex > cameraRow + screenHeight - 1) {
        cameraRow = rowIndex - screenHeight + 1;
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
    if (visibleColumn < 0 || visibleColumn >= screenWidth || visibleRow < 0 ||
        visibleRow >= screenHeight) {
        return std::nullopt;
    }
    return ScreenCell{screenLeftColumn + visibleColumn, screenTopRow + visibleRow};
}

}
