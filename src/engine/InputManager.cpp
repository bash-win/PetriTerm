#include "petriterm/engine/InputManager.hpp"

#include <ncurses.h>

namespace petriterm::engine {

namespace {

/// Decodes one raw ncurses key read into an engine KeyEvent. readStatus is the
/// get_wch return value (KEY_CODE_YES for an arrow/function key, OK for a typed
/// character); keyValue holds the key code or character accordingly.
KeyEvent decodeKeyEvent(int readStatus, wint_t keyValue) {
    if (readStatus == KEY_CODE_YES) {
        switch (static_cast<int>(keyValue)) {
            case KEY_UP:
                return {KeyCode::ArrowUp, L'\0'};
            case KEY_DOWN:
                return {KeyCode::ArrowDown, L'\0'};
            case KEY_LEFT:
                return {KeyCode::ArrowLeft, L'\0'};
            case KEY_RIGHT:
                return {KeyCode::ArrowRight, L'\0'};
            case KEY_ENTER:
                return {KeyCode::Enter, L'\0'};
            case KEY_RESIZE:
                return {KeyCode::Resize, L'\0'};
            default:
                return {KeyCode::Unknown, L'\0'};
        }
    }

    const wchar_t character = static_cast<wchar_t>(keyValue);
    switch (character) {
        case L'\n':
        case L'\r':
            return {KeyCode::Enter, L'\0'};
        case L'\x1b':
            return {KeyCode::Escape, L'\0'};
        case L' ':
            return {KeyCode::Space, L'\0'};
        default:
            return {KeyCode::Character, character};
    }
}

}

InputManager::InputManager() {
    nodelay(stdscr, TRUE);
}

void InputManager::pollPendingKeyEvents() {
    wint_t keyValue = 0;
    int readStatus = 0;
    while ((readStatus = get_wch(&keyValue)) != ERR) {
        queuedEvents.push_back(decodeKeyEvent(readStatus, keyValue));
    }
}

std::optional<KeyEvent> InputManager::takeNextKeyEvent() {
    if (queuedEvents.empty()) {
        return std::nullopt;
    }
    const KeyEvent nextEvent = queuedEvents.front();
    queuedEvents.pop_front();
    return nextEvent;
}

}
