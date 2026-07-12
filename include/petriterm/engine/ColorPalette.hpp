#pragma once

#include <array>

namespace petriterm::engine {

/// The eight base terminal colors plus a Default sentinel that maps to the
/// terminal's own default foreground/background.
enum class TerminalColor {
    Default,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
};

/// Hands out stable ncurses color-pair attributes by semantic color so callers
/// never manage raw pair numbers. Pairs are registered lazily and reused.
class ColorPalette {
public:
    /// Records whether the terminal supports color and resets the pair registry.
    /// Must be called after start_color and before any colored drawing. No-op
    /// effect on drawing if colors are unsupported.
    void initializeColorPairs();

    /// Returns the ncurses attribute value for a foreground/background pair,
    /// registering the pair on first request and reusing it thereafter. Returns
    /// a plain (uncolored) attribute if colors are unsupported or pairs are
    /// exhausted.
    int attributeForColors(TerminalColor foreground, TerminalColor background);

private:
    static constexpr int kColorCount = 9;

    bool colorsSupported = false;
    int nextPairIndex = 1;
    std::array<int, kColorCount * kColorCount> pairIndexByColorCombo{};
};

}
