#pragma once

#include <string_view>

#include "petriterm/engine/ColorPalette.hpp"

struct _win_st;
typedef struct _win_st WINDOW;

namespace petriterm::engine {

/// Thin drawing façade over one ncurses window. Confines the ncurses API to the
/// engine so game code draws through semantic operations and colors only.
class Renderer {
public:
    /// Constructs a renderer that draws into the given ncurses window using the
    /// provided palette for color lookups.
    Renderer(WINDOW* targetWindow, ColorPalette& palette);

    /// Clears the render buffer without forcing a full physical repaint. Call at
    /// the start of each frame.
    void beginFrame();

    /// Flushes the staged buffer to the terminal in one flicker-free update. Call
    /// at the end of each frame.
    void endFrame();

    /// Draws a single wide character at the given cell with optional color and
    /// attributes. Out-of-bounds coordinates are ignored, not clamped.
    void drawGlyph(int columnIndex, int rowIndex, wchar_t glyph, TerminalColor foreground,
                   TerminalColor background, int extraAttributes = 0);

    /// Draws a left-aligned string starting at the given cell, truncated to the
    /// window width. Out-of-bounds start cells are ignored.
    void drawText(int columnIndex, int rowIndex, std::string_view text,
                  TerminalColor foreground = TerminalColor::Default,
                  TerminalColor background = TerminalColor::Default,
                  int extraAttributes = 0);

    /// Draws a single-line box border around the given rectangle using
    /// box-drawing characters. Does nothing if the rectangle is smaller than 2x2.
    void drawBorderedBox(int leftColumn, int topRow, int width, int height,
                         TerminalColor borderColor = TerminalColor::Default);

    /// Fills a rectangular region with a background color and a fill glyph.
    void fillRegion(int leftColumn, int topRow, int width, int height,
                    TerminalColor background, wchar_t fillGlyph = L' ');

private:
    WINDOW* targetWindow;
    ColorPalette& palette;
};

}
