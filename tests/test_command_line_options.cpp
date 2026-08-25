#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "petriterm/game/CommandLineOptions.hpp"

using petriterm::game::CommandLineOptions;
using petriterm::game::commandLineUsageText;
using petriterm::game::parseCommandLineOptions;

namespace {

/// Parses a command line written the way it would be typed, with the program name
/// prepended the way the process receives it.
CommandLineOptions parse(std::vector<const char*> arguments) {
    arguments.insert(arguments.begin(), "petriterm");
    return parseCommandLineOptions(static_cast<int>(arguments.size()), arguments.data());
}

}

TEST_CASE("no arguments means play normally", "[cli]") {
    const CommandLineOptions options = parse({});
    REQUIRE_FALSE(options.hasError());
    REQUIRE_FALSE(options.runHeadless);
    REQUIRE_FALSE(options.showHelp);
}

TEST_CASE("headless flags parse into settings", "[cli]") {
    const CommandLineOptions options =
        parse({"--headless", "--ticks", "5000", "--seed", "7", "--census-every", "250"});
    REQUIRE_FALSE(options.hasError());
    REQUIRE(options.runHeadless);
    REQUIRE(options.ticksToRun == 5000);
    REQUIRE(options.worldSeed == 7);
    REQUIRE(options.censusIntervalInTicks == 250);
}

TEST_CASE("flag order does not matter", "[cli]") {
    const CommandLineOptions options = parse({"--seed", "3", "--headless"});
    REQUIRE_FALSE(options.hasError());
    REQUIRE(options.runHeadless);
    REQUIRE(options.worldSeed == 3);
}

TEST_CASE("help wins over everything after it", "[cli]") {
    // Asking for help with a broken command line should still print help rather
    // than the parse error, since help is what explains the error.
    const CommandLineOptions options = parse({"--help", "--ticks", "not-a-number"});
    REQUIRE(options.showHelp);
    REQUIRE_FALSE(options.hasError());
    REQUIRE_FALSE(commandLineUsageText().empty());
}

TEST_CASE("malformed arguments are rejected with a message", "[cli]") {
    SECTION("a value that is not a number") {
        REQUIRE(parse({"--ticks", "soon"}).hasError());
    }
    SECTION("trailing rubbish after a number") {
        // from_chars stops at the 'x'; accepting this would silently run 100 ticks
        // when 100x was surely a typo for something else.
        REQUIRE(parse({"--ticks", "100x"}).hasError());
    }
    SECTION("a flag whose value is missing entirely") {
        REQUIRE(parse({"--seed"}).hasError());
        REQUIRE(parse({"--headless", "--ticks"}).hasError());
    }
    SECTION("zero or negative where a positive count is needed") {
        REQUIRE(parse({"--ticks", "0"}).hasError());
        REQUIRE(parse({"--census-every", "0"}).hasError());
        REQUIRE(parse({"--ticks", "-5"}).hasError());
    }
    SECTION("an unknown flag") {
        REQUIRE(parse({"--turbo"}).hasError());
    }
}

TEST_CASE("seed zero is a valid seed", "[cli]") {
    // Unlike a tick count, zero is a perfectly good seed, so it must not be
    // rejected by the same minimum that guards the counts.
    const CommandLineOptions options = parse({"--seed", "0"});
    REQUIRE_FALSE(options.hasError());
    REQUIRE(options.worldSeed == 0);
}
