#include "petriterm/engine/Renderer.hpp"

#include <algorithm>

#include <ncurses.h>

#include "petriterm/engine/TextMeasure.hpp"

namespace petriterm::engine {

namespace {

/// Translates presentation intent into the ncurses attribute mask expressing it.
/// The single point where emphasis becomes terminal attributes, so no other file
/// needs A_BOLD or A_REVERSE in scope.
attr_t ncursesAttributesFor(TextEmphasis emphasis) {
    switch (emphasis) {
        case TextEmphasis::Normal:
            return A_NORMAL;
        case TextEmphasis::Bold:
            return A_BOLD;
        case TextEmphasis::Dim:
            return A_DIM;
        case TextEmphasis::Inverted:
            return A_REVERSE;
        case TextEmphasis::BoldInverted:
            return A_BOLD | A_REVERSE;
        case TextEmphasis::Underlined:
            return A_UNDERLINE;
    }
    return A_NORMAL;
}

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

int Renderer::widthInColumns() const {
    return getmaxx(targetWindow);
}

int Renderer::heightInRows() const {
    return getmaxy(targetWindow);
}

void Renderer::drawGlyph(int columnIndex, int rowIndex, wchar_t glyph, TextStyle style) {
    if (!isWithinWindow(targetWindow, columnIndex, rowIndex)) {
        return;
    }
    const int colorAttribute =
        palette.attributeForColors(style.foreground, style.background);
    const auto pairNumber = static_cast<short>(PAIR_NUMBER(colorAttribute));
    const wchar_t glyphBuffer[2] = {glyph, L'\0'};
    cchar_t renderedCell;
    setcchar(&renderedCell, glyphBuffer, ncursesAttributesFor(style.emphasis), pairNumber,
             nullptr);
    mvwadd_wch(targetWindow, rowIndex, columnIndex, &renderedCell);
}

void Renderer::drawText(int columnIndex, int rowIndex, std::string_view text,
                        TextStyle style) {
    if (!isWithinWindow(targetWindow, columnIndex, rowIndex)) {
        return;
    }
    const int availableColumns = getmaxx(targetWindow) - columnIndex;
    const std::string_view visibleText = truncateToDisplayWidth(text, availableColumns);
    if (visibleText.empty()) {
        return;
    }
    const int colorAttribute =
        palette.attributeForColors(style.foreground, style.background);
    const auto pairNumber = static_cast<short>(PAIR_NUMBER(colorAttribute));
    wattr_set(targetWindow, ncursesAttributesFor(style.emphasis), pairNumber, nullptr);
    mvwaddnstr(targetWindow, rowIndex, columnIndex, visibleText.data(),
               static_cast<int>(visibleText.size()));
    resetAttributes();
}

void Renderer::drawBorderedBox(int leftColumn, int topRow, int width, int height,
                               TextStyle style, std::string_view title) {
    if (width < 2 || height < 2) {
        return;
    }
    const int rightColumn = leftColumn + width - 1;
    const int bottomRow = topRow + height - 1;
    drawGlyph(leftColumn, topRow, L'┌', style);
    drawGlyph(rightColumn, topRow, L'┐', style);
    drawGlyph(leftColumn, bottomRow, L'└', style);
    drawGlyph(rightColumn, bottomRow, L'┘', style);
    for (int column = leftColumn + 1; column < rightColumn; ++column) {
        drawGlyph(column, topRow, L'─', style);
        drawGlyph(column, bottomRow, L'─', style);
    }
    for (int row = topRow + 1; row < bottomRow; ++row) {
        drawGlyph(leftColumn, row, L'│', style);
        drawGlyph(rightColumn, row, L'│', style);
    }

    if (title.empty()) {
        return;
    }
    // Inlay the title two columns in, leaving room for a space on each side of it
    // and for the corner glyphs, so a long title shortens rather than overrunning
    // the right-hand corner.
    const int titleBudget = width - 6;
    const std::string_view visibleTitle = truncateToDisplayWidth(title, titleBudget);
    if (visibleTitle.empty()) {
        return;
    }
    const int titleColumn = leftColumn + 2;
    drawGlyph(titleColumn - 1, topRow, L' ', style);
    drawText(titleColumn, topRow, visibleTitle, style);
    drawGlyph(titleColumn + displayWidthOf(visibleTitle), topRow, L' ', style);
}

void Renderer::fillRegion(int leftColumn, int topRow, int width, int height,
                          TextStyle style, wchar_t fillGlyph) {
    for (int row = topRow; row < topRow + height; ++row) {
        for (int column = leftColumn; column < leftColumn + width; ++column) {
            drawGlyph(column, row, fillGlyph, style);
        }
    }
}

void Renderer::drawHorizontalRule(int leftColumn, int rowIndex, int length,
                                  TextStyle style) {
    for (int column = leftColumn; column < leftColumn + length; ++column) {
        drawGlyph(column, rowIndex, L'─', style);
    }
}

void Renderer::drawVerticalRule(int columnIndex, int topRow, int length, TextStyle style) {
    for (int row = topRow; row < topRow + length; ++row) {
        drawGlyph(columnIndex, row, L'│', style);
    }
}

void Renderer::applyEmphasisToRegion(int leftColumn, int topRow, int width, int height,
                                     TextEmphasis emphasis) {
    const attr_t addedAttributes = ncursesAttributesFor(emphasis);
    for (int row = topRow; row < topRow + height; ++row) {
        for (int column = leftColumn; column < leftColumn + width; ++column) {
            if (!isWithinWindow(targetWindow, column, row)) {
                continue;
            }
            cchar_t existingCell;
            if (mvwin_wch(targetWindow, row, column, &existingCell) == ERR) {
                continue;
            }
            wchar_t glyphBuffer[CCHARW_MAX + 1] = {};
            attr_t existingAttributes = 0;
            short existingPair = 0;
            if (getcchar(&existingCell, glyphBuffer, &existingAttributes, &existingPair,
                         nullptr) == ERR) {
                continue;
            }
            // A never-written cell decodes to an empty string, which setcchar
            // rejects; treat it as the blank it renders as.
            if (glyphBuffer[0] == L'\0') {
                glyphBuffer[0] = L' ';
            }
            cchar_t restyledCell;
            setcchar(&restyledCell, glyphBuffer, existingAttributes | addedAttributes,
                     existingPair, nullptr);
            mvwadd_wch(targetWindow, row, column, &restyledCell);
        }
    }
}

void Renderer::showTextCursorAt(int columnIndex, int rowIndex) {
    if (!isWithinWindow(targetWindow, columnIndex, rowIndex)) {
        return;
    }
    curs_set(1);
    wmove(targetWindow, rowIndex, columnIndex);
}

void Renderer::hideTextCursor() {
    curs_set(0);
}

void Renderer::resetAttributes() {
    wattr_set(targetWindow, A_NORMAL, 0, nullptr);
}

}
