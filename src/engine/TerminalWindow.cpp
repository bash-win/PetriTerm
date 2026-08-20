#include "petriterm/engine/TerminalWindow.hpp"

#include <csignal>

#include <ncurses.h>

namespace petriterm::engine {

namespace {

/// Milliseconds ncurses waits for the remainder of an escape sequence before
/// concluding that a lone Escape was pressed. The default of 1000 makes Escape
/// feel broken - it is withheld until the next key arrives - while a value this
/// short is still far longer than the sub-millisecond gap between the bytes of a
/// real terminal's arrow-key sequence.
constexpr int kEscapeDisambiguationDelayMilliseconds = 25;

/// Restores the terminal from curses mode when a terminating signal arrives,
/// then re-raises the signal under the default handler so the process exits with
/// the conventional status for that signal.
void restoreTerminalOnSignal(int signalNumber) {
    endwin();
    std::signal(signalNumber, SIG_DFL);
    std::raise(signalNumber);
}

}

TerminalWindow::TerminalWindow() {
    if (initscr() == nullptr) {
        throw TerminalInitializationError(
            "initscr failed: standard output is not a terminal");
    }
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    set_escdelay(kEscapeDisambiguationDelayMilliseconds);
    if (has_colors()) {
        start_color();
        use_default_colors();
    }
    std::signal(SIGINT, restoreTerminalOnSignal);
    std::signal(SIGTERM, restoreTerminalOnSignal);
    ncursesActive = true;
}

TerminalWindow::~TerminalWindow() {
    if (ncursesActive) {
        std::signal(SIGINT, SIG_DFL);
        std::signal(SIGTERM, SIG_DFL);
        endwin();
        ncursesActive = false;
    }
}

TerminalDimensions TerminalWindow::currentDimensions() const {
    int rows = 0;
    int columns = 0;
    getmaxyx(stdscr, rows, columns);
    return TerminalDimensions{columns, rows};
}

WINDOW* TerminalWindow::rootWindow() const {
    return stdscr;
}

}
