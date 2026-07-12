#include "petriterm/engine/Renderer.hpp"

#include <algorithm>

#include <ncurses.h>

namespace petriterm::engine {

namespace {

/// Returns true if the given cell lies within the drawable area of the window.
bool isWithinWindow(WINDOW* window, int columnIndex, int rowIndex) {
    int rowCount = 0;
    int columnCount = 0;
    getmaxyx(window, rowCount, columnCount);
    return columnIndex >= 0 && rowIndex >= 0 && columnIndex < columnCount &&
           rowIndex < rowCount;
}

}

Renderer::Renderer(WINDOW* targetWindow, ColorPalette& palette)
    : targetWindow(targetWindow), palette(palette) {}

void Renderer::beginFrame() {
    werase(targetWindow);
}

void Renderer::endFrame() {
    wnoutrefresh(targetWindow);
    doupdate();
}

void Renderer::drawGlyph(int columnIndex, int rowIndex, wchar_t glyph,
                         TerminalColor foreground, TerminalColor background,
                         int extraAttributes) {
    if (!isWithinWindow(targetWindow, columnIndex, rowIndex)) {
        return;
    }
    const int colorAttribute = palette.attributeForColors(foreground, background);
    const short pairNumber = static_cast<short>(PAIR_NUMBER(colorAttribute));
    const wchar_t glyphBuffer[2] = {glyph, L'\0'};
    cchar_t renderedCell;
    setcchar(&renderedCell, glyphBuffer, static_cast<attr_t>(extraAttributes), pairNumber,
             nullptr);
    mvwadd_wch(targetWindow, rowIndex, columnIndex, &renderedCell);
}

void Renderer::drawText(int columnIndex, int rowIndex, std::string_view text,
                        TerminalColor foreground, TerminalColor background,
                        int extraAttributes) {
    int rowCount = 0;
    int columnCount = 0;
    getmaxyx(targetWindow, rowCount, columnCount);
    if (columnIndex < 0 || rowIndex < 0 || columnIndex >= columnCount ||
        rowIndex >= rowCount) {
        return;
    }
    const int colorAttribute = palette.attributeForColors(foreground, background);
    const short pairNumber = static_cast<short>(PAIR_NUMBER(colorAttribute));
    wattr_set(targetWindow, static_cast<attr_t>(extraAttributes), pairNumber, nullptr);
    const int availableWidth = columnCount - columnIndex;
    const int drawLength = std::min(static_cast<int>(text.size()), availableWidth);
    mvwaddnstr(targetWindow, rowIndex, columnIndex, text.data(), drawLength);
}

void Renderer::drawBorderedBox(int leftColumn, int topRow, int width, int height,
                               TerminalColor borderColor) {
    if (width < 2 || height < 2) {
        return;
    }
    const int rightColumn = leftColumn + width - 1;
    const int bottomRow = topRow + height - 1;
    drawGlyph(leftColumn, topRow, L'┌', borderColor, TerminalColor::Default);
    drawGlyph(rightColumn, topRow, L'┐', borderColor, TerminalColor::Default);
    drawGlyph(leftColumn, bottomRow, L'└', borderColor, TerminalColor::Default);
    drawGlyph(rightColumn, bottomRow, L'┘', borderColor, TerminalColor::Default);
    for (int column = leftColumn + 1; column < rightColumn; ++column) {
        drawGlyph(column, topRow, L'─', borderColor, TerminalColor::Default);
        drawGlyph(column, bottomRow, L'─', borderColor, TerminalColor::Default);
    }
    for (int row = topRow + 1; row < bottomRow; ++row) {
        drawGlyph(leftColumn, row, L'│', borderColor, TerminalColor::Default);
        drawGlyph(rightColumn, row, L'│', borderColor, TerminalColor::Default);
    }
}

void Renderer::fillRegion(int leftColumn, int topRow, int width, int height,
                          TerminalColor background, wchar_t fillGlyph) {
    for (int row = topRow; row < topRow + height; ++row) {
        for (int column = leftColumn; column < leftColumn + width; ++column) {
            drawGlyph(column, row, fillGlyph, TerminalColor::Default, background);
        }
    }
}

}
