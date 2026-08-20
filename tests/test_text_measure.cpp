#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "petriterm/engine/TextMeasure.hpp"

using petriterm::engine::DecodedCodePoint;
using petriterm::engine::decodeFirstCodePoint;
using petriterm::engine::displayWidthOf;
using petriterm::engine::displayWidthOfCodePoint;
using petriterm::engine::truncateToDisplayWidth;

TEST_CASE("ascii text measures one column per character", "[textmeasure]") {
    REQUIRE(displayWidthOf("") == 0);
    REQUIRE(displayWidthOf("hello") == 5);
    REQUIRE(displayWidthOf("CREDITS: 50") == 11);
}

TEST_CASE("the species and biome glyphs each measure one column", "[textmeasure]") {
    // Every one of these is multi-byte in UTF-8 and East Asian Ambiguous, which is
    // exactly the class wcwidth would report as two under a CJK locale.
    REQUIRE(displayWidthOf("♣") == 1);  // club, jungle fern
    REQUIRE(displayWidthOf("♠") == 1);  // spade, jungle palm
    REQUIRE(displayWidthOf("‡") == 1);  // double dagger, barrel cactus
    REQUIRE(displayWidthOf("†") == 1);  // dagger, desert aloe
    REQUIRE(displayWidthOf("∴") == 1);  // therefore, bog moss
    REQUIRE(displayWidthOf("♣♠‡†∴") == 5);
}

TEST_CASE("the box drawing and sparkline ramps each measure one column", "[textmeasure]") {
    REQUIRE(displayWidthOf("┌┐└┘─│") == 6);
    REQUIRE(displayWidthOf("▁▂▃▄▅▆▇█") == 8);
}

TEST_CASE("wide and zero width code points measure two and zero columns", "[textmeasure]") {
    REQUIRE(displayWidthOfCodePoint(U'一') == 2);          // CJK ideograph
    REQUIRE(displayWidthOfCodePoint(U'Ａ') == 2);          // fullwidth latin A
    REQUIRE(displayWidthOfCodePoint(U'가') == 2);          // Hangul syllable
    REQUIRE(displayWidthOfCodePoint(U'\U0001f600') == 2);  // emoji
    REQUIRE(displayWidthOfCodePoint(U'́') == 0);            // combining acute
    REQUIRE(displayWidthOfCodePoint(U'️') == 0);            // variation selector
    REQUIRE(displayWidthOfCodePoint(U'\n') == 0);          // control character
    REQUIRE(displayWidthOf("世界") == 4);
    REQUIRE(displayWidthOf("é") == 1);
}

TEST_CASE("decoding reports the byte length of each utf-8 sequence", "[textmeasure]") {
    const DecodedCodePoint ascii = decodeFirstCodePoint("a");
    REQUIRE(ascii.isValid);
    REQUIRE(ascii.codePoint == U'a');
    REQUIRE(ascii.byteLength == 1);

    const DecodedCodePoint threeByte = decodeFirstCodePoint("♣");
    REQUIRE(threeByte.isValid);
    REQUIRE(threeByte.codePoint == U'♣');
    REQUIRE(threeByte.byteLength == 3);

    const DecodedCodePoint fourByte = decodeFirstCodePoint("\U0001f600");
    REQUIRE(fourByte.isValid);
    REQUIRE(fourByte.codePoint == U'\U0001f600');
    REQUIRE(fourByte.byteLength == 4);

    const DecodedCodePoint empty = decodeFirstCodePoint("");
    REQUIRE_FALSE(empty.isValid);
    REQUIRE(empty.byteLength == 0);
}

TEST_CASE("invalid utf-8 always consumes one byte so callers make progress",
          "[textmeasure]") {
    SECTION("a bare continuation byte") {
        const DecodedCodePoint decoded = decodeFirstCodePoint("\x80");
        REQUIRE_FALSE(decoded.isValid);
        REQUIRE(decoded.byteLength == 1);
    }
    SECTION("a truncated sequence") {
        const DecodedCodePoint decoded = decodeFirstCodePoint("\xe2\x99");
        REQUIRE_FALSE(decoded.isValid);
        REQUIRE(decoded.byteLength == 1);
    }
    SECTION("an overlong encoding") {
        const DecodedCodePoint decoded = decodeFirstCodePoint("\xc0\x80");
        REQUIRE_FALSE(decoded.isValid);
        REQUIRE(decoded.byteLength == 1);
    }
    SECTION("an encoded surrogate half") {
        const DecodedCodePoint decoded = decodeFirstCodePoint("\xed\xa0\x80");
        REQUIRE_FALSE(decoded.isValid);
        REQUIRE(decoded.byteLength == 1);
    }
    SECTION("invalid bytes each count as one column") {
        REQUIRE(displayWidthOf("\x80\x80\x80") == 3);
    }
}

TEST_CASE("truncation never splits a multi-byte sequence", "[textmeasure]") {
    // The regression this whole module exists for: clipping by bytes would cut
    // this five-glyph, fifteen-byte string mid-sequence and corrupt the line.
    constexpr std::string_view glyphs = "♣♠‡†∴";
    REQUIRE(glyphs.size() == 15);

    REQUIRE(truncateToDisplayWidth(glyphs, 5) == glyphs);
    REQUIRE(truncateToDisplayWidth(glyphs, 99) == glyphs);
    REQUIRE(truncateToDisplayWidth(glyphs, 3) == "♣♠‡");
    REQUIRE(truncateToDisplayWidth(glyphs, 1) == "♣");

    for (int budget = 0; budget <= 6; ++budget) {
        const std::string_view prefix = truncateToDisplayWidth(glyphs, budget);
        REQUIRE(prefix.size() % 3 == 0);
        REQUIRE(displayWidthOf(prefix) <= budget);
    }
}

TEST_CASE("truncation never leaves a wide glyph half drawn", "[textmeasure]") {
    constexpr std::string_view wide = "世界";
    REQUIRE(displayWidthOf(wide) == 4);
    // A budget of three cannot fit the second two-column glyph, so it is dropped
    // entirely rather than occupying one column.
    REQUIRE(truncateToDisplayWidth(wide, 3) == "世");
    REQUIRE(truncateToDisplayWidth(wide, 4) == wide);
    REQUIRE(truncateToDisplayWidth(wide, 1) == "");
}

TEST_CASE("truncation to a non-positive width yields nothing", "[textmeasure]") {
    REQUIRE(truncateToDisplayWidth("hello", 0).empty());
    REQUIRE(truncateToDisplayWidth("hello", -1).empty());
    REQUIRE(truncateToDisplayWidth("hello", -100).empty());
    REQUIRE(truncateToDisplayWidth("", 10).empty());
}

TEST_CASE("ascii truncation cuts at the requested column", "[textmeasure]") {
    REQUIRE(truncateToDisplayWidth("hello world", 5) == "hello");
    REQUIRE(truncateToDisplayWidth("hello", 5) == "hello");
    REQUIRE(truncateToDisplayWidth("hello", 4) == "hell");
}
