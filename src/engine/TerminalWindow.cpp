#include "petriterm/engine/TerminalWindow.hpp"

#include <csignal>

#include <ncurses.h>

namespace petriterm::engine {

namespace {

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

bool TerminalWindow::waitUntilTerminalIsAtLeast(int minimumColumns, int minimumRows) {
    while (true) {
        const TerminalDimensions dimensions = currentDimensions();
        if (dimensions.columns >= minimumColumns && dimensions.rows >= minimumRows) {
            return true;
        }
        erase();
        mvprintw(0, 0, "Terminal too small.");
        mvprintw(1, 0, "Need at least %d x %d; current size is %d x %d.",
                 minimumColumns, minimumRows, dimensions.columns, dimensions.rows);
        mvprintw(2, 0, "Resize the terminal, or press q to quit.");
        refresh();
        const int keyCode = getch();
        if (keyCode == 'q' || keyCode == 'Q') {
            return false;
        }
    }
}

}
