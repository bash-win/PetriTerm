#include <catch2/catch_test_macros.hpp>

#include <ncurses.h>

#include "petriterm/engine/InputManager.hpp"

using petriterm::engine::decodeRawKeyRead;
using petriterm::engine::KeyCode;
using petriterm::engine::KeyEvent;

namespace {

/// Decodes a keypad key code, the get_wch path taken for arrows and named keys.
KeyEvent keypadKey(int keyCode) {
    return decodeRawKeyRead(KEY_CODE_YES, static_cast<std::wint_t>(keyCode));
}

/// Decodes a typed character, the other get_wch path.
KeyEvent typedCharacter(wchar_t character) {
    return decodeRawKeyRead(OK, static_cast<std::wint_t>(character));
}

}

TEST_CASE("arrow keys decode to their directions", "[input]") {
    REQUIRE(keypadKey(KEY_UP).code == KeyCode::ArrowUp);
    REQUIRE(keypadKey(KEY_DOWN).code == KeyCode::ArrowDown);
    REQUIRE(keypadKey(KEY_LEFT).code == KeyCode::ArrowLeft);
    REQUIRE(keypadKey(KEY_RIGHT).code == KeyCode::ArrowRight);
}

TEST_CASE("tab decodes to its own key code rather than a character", "[input]") {
    // Previously Tab arrived as KeyCode::Character with L'\t', forcing every
    // scene that wanted it to inspect the character field.
    const KeyEvent tab = typedCharacter(L'\t');
    REQUIRE(tab.code == KeyCode::Tab);
    REQUIRE(tab.character == L'\0');
    REQUIRE(keypadKey(KEY_BTAB).code == KeyCode::BackTab);
}

TEST_CASE("backspace decodes from all three of its encodings", "[input]") {
    // Terminals disagree: some send the terminfo kbs code, some ASCII backspace,
    // and many send ASCII delete. All three must reach the same key code or
    // text entry breaks on some terminals and not others.
    REQUIRE(keypadKey(KEY_BACKSPACE).code == KeyCode::Backspace);
    REQUIRE(typedCharacter(L'\b').code == KeyCode::Backspace);
    REQUIRE(typedCharacter(static_cast<wchar_t>(0x7F)).code == KeyCode::Backspace);
}

TEST_CASE("enter decodes from all three of its encodings", "[input]") {
    REQUIRE(keypadKey(KEY_ENTER).code == KeyCode::Enter);
    REQUIRE(typedCharacter(L'\n').code == KeyCode::Enter);
    REQUIRE(typedCharacter(L'\r').code == KeyCode::Enter);
}

TEST_CASE("navigation and editing keys decode to their key codes", "[input]") {
    REQUIRE(keypadKey(KEY_HOME).code == KeyCode::Home);
    REQUIRE(keypadKey(KEY_END).code == KeyCode::End);
    REQUIRE(keypadKey(KEY_PPAGE).code == KeyCode::PageUp);
    REQUIRE(keypadKey(KEY_NPAGE).code == KeyCode::PageDown);
    REQUIRE(keypadKey(KEY_DC).code == KeyCode::Delete);
    REQUIRE(keypadKey(KEY_RESIZE).code == KeyCode::Resize);
}

TEST_CASE("function keys carry their number", "[input]") {
    for (int number = 1; number <= 12; ++number) {
        const KeyEvent event = keypadKey(KEY_F(number));
        REQUIRE(event.code == KeyCode::FunctionKey);
        REQUIRE(event.functionKeyNumber == number);
    }
}

TEST_CASE("escape and space decode to their own key codes", "[input]") {
    REQUIRE(typedCharacter(L'\x1b').code == KeyCode::Escape);
    REQUIRE(typedCharacter(L' ').code == KeyCode::Space);
    REQUIRE(typedCharacter(L' ').character == L'\0');
}

TEST_CASE("printable characters are reported with their character", "[input]") {
    const KeyEvent lower = typedCharacter(L'q');
    REQUIRE(lower.code == KeyCode::Character);
    REQUIRE(lower.character == L'q');

    const KeyEvent digit = typedCharacter(L'7');
    REQUIRE(digit.code == KeyCode::Character);
    REQUIRE(digit.character == L'7');

    // A wide character must survive decoding intact, since species glyphs and
    // any future text entry are not ASCII.
    const KeyEvent wide = typedCharacter(L'♣');
    REQUIRE(wide.code == KeyCode::Character);
    REQUIRE(wide.character == L'♣');
}

TEST_CASE("unrecognized keypad codes fall through to Unknown", "[input]") {
    // Unknown must stay distinguishable from Escape: an unmapped keypad code is
    // ignorable, whereas Escape is a meaningful command.
    REQUIRE(keypadKey(KEY_SLEFT).code == KeyCode::Unknown);
    REQUIRE(keypadKey(KEY_F(20)).code == KeyCode::Unknown);
    REQUIRE(keypadKey(KEY_PRINT).code == KeyCode::Unknown);
}

TEST_CASE("every decoded event leaves unused fields zeroed", "[input]") {
    // Scenes switch on code alone, so a stale character or key number left in a
    // non-Character event would be a trap for later readers.
    for (const KeyEvent event :
         {keypadKey(KEY_UP), keypadKey(KEY_HOME), typedCharacter(L'\t'),
          typedCharacter(L' '), typedCharacter(L'\r'), typedCharacter(L'\x1b')}) {
        REQUIRE(event.character == L'\0');
        REQUIRE(event.functionKeyNumber == 0);
    }
    REQUIRE(keypadKey(KEY_F(3)).character == L'\0');
    REQUIRE(typedCharacter(L'a').functionKeyNumber == 0);
}
