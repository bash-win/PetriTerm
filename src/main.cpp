#include <clocale>
#include <cstdio>
#include <cstring>
#include <exception>

#include <ncurses.h>

#include "petriterm/engine/TerminalWindow.hpp"

namespace {

/// Draws a centered banner and blocks until the user presses q, demonstrating
/// that the terminal session opens and tears down cleanly. This bootstrap body
/// is replaced by the scene-driven renderer in a later milestone.
void showBannerUntilQuitRequested(const petriterm::engine::TerminalDimensions& dimensions) {
    const char* titleLine = "PetriTerm";
    const char* hintLine = "press q to quit";
    erase();
    mvprintw(dimensions.rows / 2,
             (dimensions.columns - static_cast<int>(std::strlen(titleLine))) / 2,
             "%s", titleLine);
    mvprintw(dimensions.rows / 2 + 1,
             (dimensions.columns - static_cast<int>(std::strlen(hintLine))) / 2,
             "%s", hintLine);
    refresh();
    int keyCode = getch();
    while (keyCode != 'q' && keyCode != 'Q') {
        keyCode = getch();
    }
}

}

int main() {
    std::setlocale(LC_ALL, "");
    try {
        petriterm::engine::TerminalWindow terminal;
        if (!terminal.waitUntilTerminalIsAtLeast(80, 24)) {
            return 0;
        }
        showBannerUntilQuitRequested(terminal.currentDimensions());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "PetriTerm fatal error: %s\n", error.what());
        return 1;
    }
    return 0;
}
