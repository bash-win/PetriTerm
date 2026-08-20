#include "petriterm/engine/TextMeasure.hpp"

#include <array>
#include <cstddef>

namespace petriterm::engine {

namespace {

/// An inclusive code-point range, used for the width tables below.
struct CodePointRange {
    char32_t firstCodePoint;
    char32_t lastCodePoint;
};

/// Returns true if the code point falls in one of the ranges, which are kept in
/// ascending order so the search can stop early.
bool rangesContain(const CodePointRange* ranges, std::size_t rangeCount,
                   char32_t codePoint) {
    for (std::size_t index = 0; index < rangeCount; ++index) {
        if (codePoint < ranges[index].firstCodePoint) {
            return false;
        }
        if (codePoint <= ranges[index].lastCodePoint) {
            return true;
        }
    }
    return false;
}

/// Code points that occupy no columns: combining marks, joiners, and variation
/// selectors, all of which a terminal composes onto the preceding cell.
constexpr std::array<CodePointRange, 8> kZeroWidthRanges{{
    {0x0300, 0x036F},  // combining diacritical marks
    {0x0483, 0x0489},  // Cyrillic combining marks
    {0x0591, 0x05BD},  // Hebrew points
    {0x0610, 0x061A},  // Arabic marks
    {0x064B, 0x065F},  // Arabic vowel marks
    {0x200B, 0x200F},  // zero-width space through right-to-left mark
    {0x20D0, 0x20F0},  // combining marks for symbols
    {0xFE00, 0xFE0F},  // variation selectors
}};

/// Code points that occupy two columns: the East Asian Wide and Fullwidth ranges
/// plus the emoji blocks terminals render double-wide.
constexpr std::array<CodePointRange, 14> kWideRanges{{
    {0x1100, 0x115F},    // Hangul Jamo initial consonants
    {0x2E80, 0x303E},    // CJK radicals through CJK symbols
    {0x3041, 0x33FF},    // Hiragana through CJK compatibility
    {0x3400, 0x4DBF},    // CJK unified ideographs extension A
    {0x4E00, 0x9FFF},    // CJK unified ideographs
    {0xA000, 0xA4CF},    // Yi syllables
    {0xAC00, 0xD7A3},    // Hangul syllables
    {0xF900, 0xFAFF},    // CJK compatibility ideographs
    {0xFE10, 0xFE19},    // vertical forms
    {0xFE30, 0xFE6F},    // CJK compatibility forms
    {0xFF00, 0xFF60},    // fullwidth forms
    {0xFFE0, 0xFFE6},    // fullwidth signs
    {0x1F300, 0x1F9FF},  // emoji and pictographs
    {0x20000, 0x3FFFD},  // CJK ideograph planes
}};

}

DecodedCodePoint decodeFirstCodePoint(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }

    const auto leadByte = static_cast<unsigned char>(utf8[0]);

    if (leadByte < 0x80) {
        return {leadByte, 1, true};
    }

    int sequenceLength = 0;
    char32_t codePoint = 0;
    if ((leadByte & 0xE0) == 0xC0) {
        sequenceLength = 2;
        codePoint = leadByte & 0x1FU;
    } else if ((leadByte & 0xF0) == 0xE0) {
        sequenceLength = 3;
        codePoint = leadByte & 0x0FU;
    } else if ((leadByte & 0xF8) == 0xF0) {
        sequenceLength = 4;
        codePoint = leadByte & 0x07U;
    } else {
        // A continuation byte or an out-of-range lead byte cannot start a
        // sequence. Consume exactly one byte so the caller still advances.
        return {0, 1, false};
    }

    if (utf8.size() < static_cast<std::size_t>(sequenceLength)) {
        return {0, 1, false};
    }

    for (int index = 1; index < sequenceLength; ++index) {
        const auto continuationByte = static_cast<unsigned char>(utf8[index]);
        if ((continuationByte & 0xC0) != 0x80) {
            return {0, 1, false};
        }
        codePoint = (codePoint << 6U) | (continuationByte & 0x3FU);
    }

    // Reject overlong encodings, surrogates, and out-of-range values so an
    // invalid sequence can never be mistaken for a printable glyph.
    const bool isOverlong = (sequenceLength == 2 && codePoint < 0x80) ||
                            (sequenceLength == 3 && codePoint < 0x800) ||
                            (sequenceLength == 4 && codePoint < 0x10000);
    const bool isSurrogate = codePoint >= 0xD800 && codePoint <= 0xDFFF;
    if (isOverlong || isSurrogate || codePoint > 0x10FFFF) {
        return {0, 1, false};
    }

    return {codePoint, sequenceLength, true};
}

int displayWidthOfCodePoint(char32_t codePoint) {
    if (codePoint == 0) {
        return 0;
    }
    if (codePoint < 0x20 || (codePoint >= 0x7F && codePoint < 0xA0)) {
        return 0;
    }
    if (rangesContain(kZeroWidthRanges.data(), kZeroWidthRanges.size(), codePoint)) {
        return 0;
    }
    if (rangesContain(kWideRanges.data(), kWideRanges.size(), codePoint)) {
        return 2;
    }
    return 1;
}

int displayWidthOf(std::string_view utf8) {
    int totalWidth = 0;
    std::string_view remaining = utf8;
    while (!remaining.empty()) {
        const DecodedCodePoint decoded = decodeFirstCodePoint(remaining);
        totalWidth += decoded.isValid ? displayWidthOfCodePoint(decoded.codePoint) : 1;
        remaining.remove_prefix(static_cast<std::size_t>(decoded.byteLength));
    }
    return totalWidth;
}

std::string_view truncateToDisplayWidth(std::string_view utf8, int maximumColumns) {
    if (maximumColumns <= 0) {
        return utf8.substr(0, 0);
    }

    int usedColumns = 0;
    std::size_t byteOffset = 0;
    while (byteOffset < utf8.size()) {
        const DecodedCodePoint decoded = decodeFirstCodePoint(utf8.substr(byteOffset));
        const int glyphWidth =
            decoded.isValid ? displayWidthOfCodePoint(decoded.codePoint) : 1;
        if (usedColumns + glyphWidth > maximumColumns) {
            break;
        }
        usedColumns += glyphWidth;
        byteOffset += static_cast<std::size_t>(decoded.byteLength);
    }
    return utf8.substr(0, byteOffset);
}

}
