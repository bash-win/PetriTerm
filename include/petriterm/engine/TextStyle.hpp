#pragma once

#include "petriterm/engine/ColorPalette.hpp"

namespace petriterm::engine {

/// Presentation intent rather than a raw ncurses attribute mask. Game code says
/// what a run of text means and the Renderer decides how to express it, which is
/// what lets the engine keep ncurses.h out of every scene.
enum class TextEmphasis {
    Normal,
    Bold,
    Dim,
    Inverted,
    BoldInverted,
    Underlined,
};

/// The complete appearance of a drawn glyph or string. Aggregated into one small
/// value so every drawing call takes a single style argument instead of three
/// positional parameters that are easy to transpose.
struct TextStyle {
    TerminalColor foreground = TerminalColor::Default;
    TerminalColor background = TerminalColor::Default;
    TextEmphasis emphasis = TextEmphasis::Normal;
};

}
