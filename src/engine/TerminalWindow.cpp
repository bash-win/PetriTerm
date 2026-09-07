#include "petriterm/engine/TerminalWindow.hpp"

#include <csignal>
#include <optional>

#include <sys/ioctl.h>
#include <unistd.h>

#include "petriterm/engine/Curses.hpp"

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

/// Asks the kernel for the window size of the terminal on standard output, or
/// std::nullopt if it cannot say.
///
/// Asked directly rather than read from curses because curses is the thing being
/// corrected here. Some terminals and multiplexers report a zero dimension while
/// a resize is in flight, which is treated the same as no answer: keeping the
/// previous size for one more frame is always better than resizing the screen to
/// nothing.
std::optional<TerminalDimensions> queryTerminalSizeFromKernel() {
    winsize windowSize{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &windowSize) == -1) {
        return std::nullopt;
    }
    if (windowSize.ws_col == 0 || windowSize.ws_row == 0) {
        return std::nullopt;
    }
    return TerminalDimensions{static_cast<int>(windowSize.ws_col),
                              static_cast<int>(windowSize.ws_row)};
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

TerminalDimensions TerminalWindow::adoptResizedTerminal() {
    // Idempotent by design: ncurses' own SIGWINCH handling already calls
    // resizeterm before it delivers KEY_RESIZE on most builds, so this usually
    // finds the sizes already in agreement and does nothing. It is here for the
    // builds and platforms where that is not true, and so the resize path does
    // not depend on which of those this is.
    if (const std::optional<TerminalDimensions> liveSize = queryTerminalSizeFromKernel()) {
        const TerminalDimensions cursesSize = currentDimensions();
        if (liveSize->rows != cursesSize.rows || liveSize->columns != cursesSize.columns) {
            resizeterm(liveSize->rows, liveSize->columns);
        }
    }
    clearok(stdscr, TRUE);
    return currentDimensions();
}

WINDOW* TerminalWindow::rootWindow() const {
    return stdscr;
}

}
