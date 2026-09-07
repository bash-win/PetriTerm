#pragma once

#include <optional>

#include "petriterm/engine/ScreenRegion.hpp"

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
    /// in the given screen region (absolute terminal coordinates). The world size
    /// is fixed for the viewport's life; the screen region is not, because the
    /// terminal it is measured against can be resized under us.
    Viewport(int worldWidthInTiles, int worldHeightInTiles,
             const engine::ScreenRegion& screenRegion);

    /// Moves and resizes the screen region this camera projects into, called on
    /// every relayout.
    ///
    /// The camera is re-clamped rather than recentred: growing the view past the
    /// world edge pulls the camera back so no out-of-world tiles come into view,
    /// and shrinking it leaves the top-left tile where it was. Holding the
    /// top-left fixed is what makes a resize feel like the window changing size
    /// over a stationary map rather than the map jumping.
    void setScreenRegion(const engine::ScreenRegion& screenRegion);

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
    int scrollSpanInColumns() const;
    int scrollSpanInRows() const;
    int maximumCameraColumn() const;
    int maximumCameraRow() const;
    void clampCameraToWorldBounds();

    int worldWidthInTiles;
    int worldHeightInTiles;
    engine::ScreenRegion screenRegion;
    int cameraColumn = 0;
    int cameraRow = 0;
};

}
