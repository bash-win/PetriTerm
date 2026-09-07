#pragma once

#include <stdexcept>

struct _win_st;
typedef struct _win_st WINDOW;

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
    /// starts color support, and shortens the escape-sequence disambiguation
    /// delay. Installs SIGINT/SIGTERM handlers that restore the terminal before
    /// the process exits. Throws TerminalInitializationError if the standard
    /// streams are not attached to a terminal.
    TerminalWindow();

    /// Restores the terminal to its pre-curses state exactly once and returns
    /// SIGINT/SIGTERM to their default handlers.
    ~TerminalWindow();

    TerminalWindow(const TerminalWindow&) = delete;
    TerminalWindow& operator=(const TerminalWindow&) = delete;
    TerminalWindow(TerminalWindow&&) = delete;
    TerminalWindow& operator=(TerminalWindow&&) = delete;

    /// Returns the size curses currently believes the terminal is, in character
    /// cells, re-queried each call.
    ///
    /// This tracks the curses screen rather than the terminal, and the two differ
    /// between the moment the window changes and the moment the resize is adopted
    /// - so a caller that wants the live size calls adoptResizedTerminal first.
    TerminalDimensions currentDimensions() const;

    /// Re-synchronizes the curses screen with the terminal's real size and
    /// returns the adopted dimensions.
    ///
    /// Call this on a KeyCode::Resize event. Until it runs, stdscr keeps its old
    /// size: writes beyond it are silently clipped and everything laid out
    /// against the old size stays where it was, which is what makes an unhandled
    /// resize look like a corrupted screen rather than a stale one.
    ///
    /// Also marks the screen for a full repaint. Curses diffs each frame against
    /// its picture of what the terminal is showing, and a resize invalidates that
    /// picture in ways it cannot model - the terminal has reflowed or discarded
    /// content on its own - so the next frame has to redraw every cell instead of
    /// only the ones the game changed.
    TerminalDimensions adoptResizedTerminal();

    /// Returns the full-screen window ncurses created at initialization, for
    /// handing to a Renderer. Exposed here, from the class that already owns the
    /// curses session, so no caller has to name stdscr and pull ncurses.h into
    /// game code.
    WINDOW* rootWindow() const;

private:
    bool ncursesActive = false;
};

}
