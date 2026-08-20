#pragma once

#include <string_view>

#include "petriterm/engine/ColorPalette.hpp"
#include "petriterm/engine/TextStyle.hpp"

struct _win_st;
typedef struct _win_st WINDOW;

namespace petriterm::engine {

/// Thin drawing façade over one ncurses window. Confines the ncurses API to the
/// engine so game code draws through semantic operations, colors, and emphasis
/// only. Every primitive clips to the window rather than clamping or wrapping, so
/// a partially off-screen panel is safe to draw.
class Renderer {
public:
    /// Constructs a renderer that draws into the given ncurses window using the
    /// provided palette for color lookups.
    Renderer(WINDOW* targetWindow, ColorPalette& palette);

    /// Clears the render buffer without forcing a full physical repaint. Owned by
    /// the SceneManager rather than individual scenes, so a stack of scenes can
    /// share one frame and overlays can draw over what is beneath them.
    void beginFrame();

    /// Flushes the staged buffer to the terminal in one flicker-free update.
    void endFrame();

    /// Returns the drawable size of the window, so layout code can query the
    /// surface it is drawing into without reaching for ncurses.
    int widthInColumns() const;
    int heightInRows() const;

    /// Draws a single wide character at the given cell. Out-of-bounds coordinates
    /// are ignored, not clamped.
    void drawGlyph(int columnIndex, int rowIndex, wchar_t glyph, TextStyle style);

    /// Draws a left-aligned string starting at the given cell, truncated to the
    /// remaining window width by display columns rather than by bytes so a
    /// multi-byte glyph is never cut in half.
    void drawText(int columnIndex, int rowIndex, std::string_view text,
                  TextStyle style = {});

    /// Draws a single-line box border around the given rectangle, with an optional
    /// title inlaid into the top edge. Does nothing if the rectangle is smaller
    /// than 2x2; a title too long for the edge is truncated.
    void drawBorderedBox(int leftColumn, int topRow, int width, int height,
                         TextStyle style = {}, std::string_view title = {});

    /// Fills a rectangular region with the style's colors and a fill glyph.
    void fillRegion(int leftColumn, int topRow, int width, int height, TextStyle style = {},
                    wchar_t fillGlyph = L' ');

    /// Draws a run of horizontal or vertical line glyphs, for separating panes
    /// without drawing a full box around either side.
    void drawHorizontalRule(int leftColumn, int rowIndex, int length, TextStyle style = {});
    void drawVerticalRule(int columnIndex, int topRow, int length, TextStyle style = {});

    /// Stamps additional emphasis onto already-staged cells, preserving their
    /// glyphs and colors. This is how a modal dims the screen behind it: the
    /// covered scene renders normally and the overlay dims it afterwards.
    void applyEmphasisToRegion(int leftColumn, int topRow, int width, int height,
                               TextEmphasis emphasis);

    /// Shows or hides the hardware text cursor. Used by text-entry fields, which
    /// need a real caret the terminal blinks rather than a drawn stand-in.
    void showTextCursorAt(int columnIndex, int rowIndex);
    void hideTextCursor();

    /// Clears any lingering attribute state on the window. Called at the end of
    /// each text run so a later raw ncurses write cannot inherit a stale style.
    void resetAttributes();

private:
    WINDOW* targetWindow;
    ColorPalette& palette;
};

}
