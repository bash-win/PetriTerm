#pragma once

#include <optional>

namespace petriterm::game {

/// An on-screen character cell, in absolute terminal coordinates.
struct ScreenCell {
    int columnIndex = 0;
    int rowIndex = 0;
};

/// A scrollable camera over a world that may be larger than its screen region.
/// Holds only the mapping between tile coordinates and screen cells; it does no
/// drawing itself, so callers iterate the visible tiles and render them through
/// the engine renderer. The camera is always clamped so the view stays within
/// the world bounds.
class Viewport {
public:
    /// Constructs a viewport for a world of the given tile dimensions displayed
    /// in the given screen region (absolute terminal coordinates).
    Viewport(int worldWidthInTiles, int worldHeightInTiles, int screenLeftColumn,
             int screenTopRow, int screenWidth, int screenHeight);

    /// Scrolls the camera by the given tile deltas, clamped so the view stays
    /// within the world bounds.
    void scrollByTiles(int columnDelta, int rowDelta);

    /// Scrolls the minimum amount needed to bring the given tile into view, used
    /// to follow the placement cursor.
    void ensureTileVisible(int columnIndex, int rowIndex);

    /// Converts a tile coordinate to its on-screen cell, or std::nullopt if the
    /// tile lies outside the world or is currently scrolled off-screen.
    std::optional<ScreenCell> tileToScreenCell(int columnIndex, int rowIndex) const;

    /// The tile coordinate shown at the top-left of the screen region.
    int cameraColumnIndex() const { return cameraColumn; }
    int cameraRowIndex() const { return cameraRow; }

    /// The number of tiles visible along each axis (the screen region, capped by
    /// the world size). Callers iterate these to draw the visible region.
    int visibleWidthInTiles() const;
    int visibleHeightInTiles() const;

private:
    int maximumCameraColumn() const;
    int maximumCameraRow() const;
    void clampCameraToWorldBounds();

    int worldWidthInTiles;
    int worldHeightInTiles;
    int screenLeftColumn;
    int screenTopRow;
    int screenWidth;
    int screenHeight;
    int cameraColumn = 0;
    int cameraRow = 0;
};

}
