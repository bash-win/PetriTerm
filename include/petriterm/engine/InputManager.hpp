#pragma once

#include <deque>
#include <optional>

namespace petriterm::engine {

/// Engine-level classification of a key press, decoupled from raw ncurses key
/// codes so scenes and any future input backend (including mouse) stay
/// ncurses-agnostic.
enum class KeyCode {
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
    Enter,
    Escape,
    Space,
    Character,
    Resize,
    Unknown,
};

/// One decoded key press. The character field is meaningful only when code is
/// KeyCode::Character; it is L'\0' for every other code.
struct KeyEvent {
    KeyCode code = KeyCode::Unknown;
    wchar_t character = L'\0';
};

/// Non-blocking keyboard reader that decodes raw ncurses key codes into
/// KeyEvents and queues them for the active scene to drain each frame.
class InputManager {
public:
    /// Switches the terminal to non-blocking key reads so polling never stalls
    /// the render loop. Precondition: the ncurses session (TerminalWindow) is
    /// already initialized, which also enables keypad decoding of arrow keys.
    InputManager();

    /// Reads all keys currently available without blocking and appends them to
    /// the internal event queue. Call once per frame.
    void pollPendingKeyEvents();

    /// Removes and returns the next queued key event, or std::nullopt if the
    /// queue is empty.
    std::optional<KeyEvent> takeNextKeyEvent();

private:
    std::deque<KeyEvent> queuedEvents;
};

}
