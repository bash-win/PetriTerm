#include <clocale>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string_view>

#include <ncurses.h>

#include "petriterm/engine/ColorPalette.hpp"
#include "petriterm/engine/Renderer.hpp"
#include "petriterm/engine/TerminalWindow.hpp"

namespace {

/// Returns the leftmost column that centers a string of the given length within
/// the given total width.
int centeredColumnFor(int totalWidth, std::string_view text) {
    return (totalWidth - static_cast<int>(text.size())) / 2;
}

/// Draws the PetriTerm welcome screen: a bordered frame with a centered title,
/// subtitle, and quit hint. Rebuilt each frame so terminal resizes recenter it.
void drawWelcomeScreen(petriterm::engine::Renderer& renderer,
                       const petriterm::engine::TerminalDimensions& dimensions) {
    using petriterm::engine::TerminalColor;
    constexpr std::string_view title = "PetriTerm";
    constexpr std::string_view subtitle = "Terminal Ecology Simulator";
    constexpr std::string_view hint = "press q to quit";
    const int centerRow = dimensions.rows / 2;

    renderer.beginFrame();
    renderer.drawBorderedBox(0, 0, dimensions.columns, dimensions.rows,
                             TerminalColor::Green);
    renderer.drawText(centeredColumnFor(dimensions.columns, title), centerRow - 1,
                      title, TerminalColor::Green, TerminalColor::Default, A_BOLD);
    renderer.drawText(centeredColumnFor(dimensions.columns, subtitle), centerRow,
                      subtitle, TerminalColor::Cyan);
    renderer.drawText(centeredColumnFor(dimensions.columns, hint), centerRow + 2,
                      hint, TerminalColor::Yellow);
    renderer.endFrame();
}

}

int main() {
    std::setlocale(LC_ALL, "");
    try {
        petriterm::engine::TerminalWindow terminal;
        if (!terminal.waitUntilTerminalIsAtLeast(80, 24)) {
            return 0;
        }
        petriterm::engine::ColorPalette palette;
        palette.initializeColorPairs();
        petriterm::engine::Renderer renderer(stdscr, palette);

        bool quitRequested = false;
        while (!quitRequested) {
            drawWelcomeScreen(renderer, terminal.currentDimensions());
            const int keyCode = getch();
            if (keyCode == 'q' || keyCode == 'Q') {
                quitRequested = true;
            }
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "PetriTerm fatal error: %s\n", error.what());
        return 1;
    }
    return 0;
}
