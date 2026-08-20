#pragma once

#include <string_view>

namespace petriterm::engine {

/// One decoded code point together with the number of bytes it occupied, so a
/// caller can walk a UTF-8 string without decoding it twice.
struct DecodedCodePoint {
    char32_t codePoint = 0;
    int byteLength = 0;
    bool isValid = false;
};

/// Decodes the first code point of the given UTF-8 text. An invalid or truncated
/// sequence reports one byte consumed and isValid false, so a caller always makes
/// forward progress and a corrupt byte can never cause an infinite loop.
DecodedCodePoint decodeFirstCodePoint(std::string_view utf8);

/// Returns the terminal columns one code point occupies: zero for control
/// characters and combining marks, two for the East Asian Wide and Fullwidth
/// ranges, one otherwise.
///
/// Deliberately hand-rolled rather than delegating to wcwidth. wcwidth depends on
/// the active locale, and it classifies the box-drawing, card-suit, and block
/// glyphs this game draws as East Asian Ambiguous, so under a CJK locale it would
/// report width two and silently shear every line of the layout. Owning the table
/// makes the width of our own glyphs a fixed property of the build.
int displayWidthOfCodePoint(char32_t codePoint);

/// Returns the total terminal columns the UTF-8 text occupies. An invalid byte
/// counts as one column, matching the replacement character a terminal shows.
int displayWidthOf(std::string_view utf8);

/// Returns the longest prefix of the text fitting in the given column budget,
/// never splitting a multi-byte sequence and never leaving a wide glyph half
/// drawn. Returns an empty view for a non-positive budget.
std::string_view truncateToDisplayWidth(std::string_view utf8, int maximumColumns);

}
