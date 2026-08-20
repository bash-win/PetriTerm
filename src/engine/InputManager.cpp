#include "petriterm/engine/InputManager.hpp"

#include <ncurses.h>

namespace petriterm::engine {

namespace {

/// The highest function key the decoder reports. ncurses defines codes well past
/// this, but no terminal reliably delivers them and nothing in the game binds
/// them, so the table stops where portability does.
constexpr int kHighestFunctionKey = 12;

/// The ASCII delete character, which most terminals send for the Backspace key
/// instead of the terminfo kbs code. Accepting all three spellings is what makes
/// Backspace work across terminals.
constexpr int kAsciiDelete = 0x7F;

}

KeyEvent decodeRawKeyRead(int readStatus, std::wint_t keyValue) {
    if (readStatus == KEY_CODE_YES) {
        const int keyCode = static_cast<int>(keyValue);
        if (keyCode >= KEY_F(1) && keyCode <= KEY_F(kHighestFunctionKey)) {
            return {KeyCode::FunctionKey, L'\0', keyCode - KEY_F0};
        }
        switch (keyCode) {
            case KEY_UP:
                return {KeyCode::ArrowUp, L'\0', 0};
            case KEY_DOWN:
                return {KeyCode::ArrowDown, L'\0', 0};
            case KEY_LEFT:
                return {KeyCode::ArrowLeft, L'\0', 0};
            case KEY_RIGHT:
                return {KeyCode::ArrowRight, L'\0', 0};
            case KEY_ENTER:
                return {KeyCode::Enter, L'\0', 0};
            case KEY_BTAB:
                return {KeyCode::BackTab, L'\0', 0};
            case KEY_BACKSPACE:
                return {KeyCode::Backspace, L'\0', 0};
            case KEY_DC:
                return {KeyCode::Delete, L'\0', 0};
            case KEY_HOME:
                return {KeyCode::Home, L'\0', 0};
            case KEY_END:
                return {KeyCode::End, L'\0', 0};
            case KEY_PPAGE:
                return {KeyCode::PageUp, L'\0', 0};
            case KEY_NPAGE:
                return {KeyCode::PageDown, L'\0', 0};
            case KEY_RESIZE:
                return {KeyCode::Resize, L'\0', 0};
            default:
                return {KeyCode::Unknown, L'\0', 0};
        }
    }

    const wchar_t character = static_cast<wchar_t>(keyValue);
    switch (character) {
        case L'\n':
        case L'\r':
            return {KeyCode::Enter, L'\0', 0};
        case L'\x1b':
            return {KeyCode::Escape, L'\0', 0};
        case L' ':
            return {KeyCode::Space, L'\0', 0};
        case L'\t':
            return {KeyCode::Tab, L'\0', 0};
        case L'\b':
        case static_cast<wchar_t>(kAsciiDelete):
            return {KeyCode::Backspace, L'\0', 0};
        default:
            return {KeyCode::Character, character, 0};
    }
}

InputManager::InputManager() {
    nodelay(stdscr, TRUE);
}

void InputManager::pollPendingKeyEvents() {
    wint_t keyValue = 0;
    int readStatus = 0;
    while ((readStatus = get_wch(&keyValue)) != ERR) {
        queuedEvents.push_back(decodeRawKeyRead(readStatus, keyValue));
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
