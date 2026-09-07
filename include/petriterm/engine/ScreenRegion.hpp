#pragma once

#include <algorithm>

namespace petriterm::engine {

/// A rectangle of character cells in absolute terminal coordinates.
///
/// Exists so a surface can be passed around as one value that survives a resize,
/// rather than as four ints captured once at construction. Layout is expressed by
/// carving a smaller region out of a larger one, which is why the accessors below
/// return new regions instead of mutating in place: a scene can re-derive its
/// panes from the screen region on every relayout with no state to keep in sync.
///
/// A default-constructed region is empty rather than one cell, so "no surface
/// assigned yet" is distinguishable from "a surface one cell across".
struct ScreenRegion {
    int leftColumn = 0;
    int topRow = 0;
    int widthInColumns = 0;
    int heightInRows = 0;

    /// True when the region has no cells, and so nothing drawn into it can be
    /// seen. Callers check this before laying out inside a region, because
    /// subtracting a fixed-height bar from a short terminal can leave nothing.
    constexpr bool isEmpty() const { return widthInColumns <= 0 || heightInRows <= 0; }

    /// The last row and column inside the region. Undefined for an empty region,
    /// which is why callers reach for these only after checking isEmpty().
    constexpr int bottomRow() const { return topRow + heightInRows - 1; }
    constexpr int rightColumn() const { return leftColumn + widthInColumns - 1; }

    /// The region with the bottom rowCount rows removed, for reserving a
    /// status or help bar along the bottom edge. Clamped at zero height rather
    /// than going negative, so reserving more rows than the region has yields an
    /// empty region instead of an inverted one.
    constexpr ScreenRegion withoutBottomRows(int rowCount) const {
        return ScreenRegion{leftColumn, topRow, widthInColumns,
                            std::max(0, heightInRows - std::max(0, rowCount))};
    }

    /// True if the two regions cover exactly the same cells. Scenes compare the
    /// incoming region against the one they last laid out for, so a relayout that
    /// changes nothing costs nothing.
    constexpr bool operator==(const ScreenRegion& other) const = default;
};

}
