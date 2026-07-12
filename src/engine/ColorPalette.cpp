#include "petriterm/engine/ColorPalette.hpp"

#include <ncurses.h>

namespace petriterm::engine {

namespace {

/// Maps a semantic terminal color to its ncurses color number, using -1 for the
/// Default sentinel so use_default_colors supplies the terminal's own default.
short toNcursesColor(TerminalColor color) {
    switch (color) {
        case TerminalColor::Default:
            return -1;
        case TerminalColor::Black:
            return COLOR_BLACK;
        case TerminalColor::Red:
            return COLOR_RED;
        case TerminalColor::Green:
            return COLOR_GREEN;
        case TerminalColor::Yellow:
            return COLOR_YELLOW;
        case TerminalColor::Blue:
            return COLOR_BLUE;
        case TerminalColor::Magenta:
            return COLOR_MAGENTA;
        case TerminalColor::Cyan:
            return COLOR_CYAN;
        case TerminalColor::White:
            return COLOR_WHITE;
    }
    return -1;
}

}

void ColorPalette::initializeColorPairs() {
    colorsSupported = has_colors();
    nextPairIndex = 1;
    pairIndexByColorCombo.fill(0);
}

int ColorPalette::attributeForColors(TerminalColor foreground, TerminalColor background) {
    if (!colorsSupported) {
        return 0;
    }
    const int comboIndex =
        static_cast<int>(foreground) * kColorCount + static_cast<int>(background);
    int pairIndex = pairIndexByColorCombo[static_cast<std::size_t>(comboIndex)];
    if (pairIndex == 0) {
        if (nextPairIndex >= COLOR_PAIRS) {
            return 0;
        }
        pairIndex = nextPairIndex;
        ++nextPairIndex;
        init_pair(static_cast<short>(pairIndex), toNcursesColor(foreground),
                  toNcursesColor(background));
        pairIndexByColorCombo[static_cast<std::size_t>(comboIndex)] = pairIndex;
    }
    return COLOR_PAIR(pairIndex);
}

}
