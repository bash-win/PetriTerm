#pragma once

#include <stdexcept>

namespace petriterm::engine {

/// Thrown when ncurses initialization fails and the terminal cannot enter its
/// curses drawing mode.
class TerminalInitializationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Terminal size in character cells.
struct TerminalDimensions {
    int columns;
    int rows;
};

/// RAII owner of the ncurses session. Initializes curses on construction and
/// guarantees the terminal is restored exactly once, whether by normal
/// destruction or by a terminating signal.
class TerminalWindow {
public:
    /// Initializes ncurses, enters raw single-key input mode, hides the cursor,
    /// and starts color support. Installs SIGINT/SIGTERM handlers that restore
    /// the terminal before the process exits. Throws TerminalInitializationError
    /// if the standard streams are not attached to a terminal.
    TerminalWindow();

    /// Restores the terminal to its pre-curses state exactly once and returns
    /// SIGINT/SIGTERM to their default handlers.
    ~TerminalWindow();

    TerminalWindow(const TerminalWindow&) = delete;
    TerminalWindow& operator=(const TerminalWindow&) = delete;
    TerminalWindow(TerminalWindow&&) = delete;
    TerminalWindow& operator=(TerminalWindow&&) = delete;

    /// Returns the current terminal size in character cells, re-queried each call
    /// so callers observe resizes immediately.
    TerminalDimensions currentDimensions() const;

    /// Blocks until the terminal has at least the given minimum dimensions,
    /// drawing a resize prompt until satisfied. Returns false if the user presses
    /// q to quit instead of resizing.
    bool waitUntilTerminalIsAtLeast(int minimumColumns, int minimumRows);

private:
    bool ncursesActive = false;
};

}
