#include "petriterm/game/CommandLineOptions.hpp"

#include <charconv>
#include <cstring>
#include <format>
#include <string_view>
#include <system_error>

namespace petriterm::game {

namespace {

/// Parses a whole-number argument, returning false if the text is missing, is not
/// a number, has trailing rubbish, or is below the given minimum.
///
/// Takes the raw argv pointer rather than a string_view because from_chars wants a
/// begin and an end: argv strings are NUL-terminated by definition, so strlen
/// gives a real end pointer, where a string_view would only offer data() and leave
/// the termination an assumption. Uses from_chars rather than stoi so that
/// "100abc" is rejected instead of quietly parsing as 100.
template <typename IntegerType>
bool parseWholeNumber(const char* text, IntegerType minimumValue,
                      IntegerType& destination) {
    if (text == nullptr) {
        return false;
    }
    const char* const textEnd = text + std::strlen(text);
    IntegerType parsedValue{};
    const std::from_chars_result result = std::from_chars(text, textEnd, parsedValue);
    if (result.ec != std::errc{} || result.ptr != textEnd || parsedValue < minimumValue) {
        return false;
    }
    destination = parsedValue;
    return true;
}

}

CommandLineOptions parseCommandLineOptions(int argumentCount,
                                           const char* const* argumentValues) {
    CommandLineOptions options;

    // Returns the argument after the given one, or nullptr when the flag is last
    // on the line and its value is simply missing.
    const auto valueFollowing = [&](int index) -> const char* {
        return index + 1 < argumentCount ? argumentValues[index + 1] : nullptr;
    };

    for (int index = 1; index < argumentCount; ++index) {
        const std::string_view argument(argumentValues[index]);

        if (argument == "--help" || argument == "-h") {
            options.showHelp = true;
            return options;
        }
        if (argument == "--headless") {
            options.runHeadless = true;
            continue;
        }

        const char* const value = valueFollowing(index);
        if (argument == "--ticks") {
            if (!parseWholeNumber(value, 1, options.ticksToRun)) {
                options.errorMessage = "--ticks needs a whole number of at least 1";
                return options;
            }
            ++index;
            continue;
        }
        if (argument == "--census-every") {
            if (!parseWholeNumber(value, 1, options.censusIntervalInTicks)) {
                options.errorMessage = "--census-every needs a whole number of at least 1";
                return options;
            }
            ++index;
            continue;
        }
        if (argument == "--seed") {
            if (!parseWholeNumber<std::uint64_t>(value, 0, options.worldSeed)) {
                options.errorMessage = "--seed needs a whole number";
                return options;
            }
            ++index;
            continue;
        }

        options.errorMessage = std::format("unrecognized argument '{}'", argument);
        return options;
    }
    return options;
}

std::string commandLineUsageText() {
    return "PetriTerm - a terminal ecosystem simulation\n"
           "\n"
           "Usage: petriterm [options]\n"
           "\n"
           "With no options, plays normally and needs a terminal at least 80x24.\n"
           "\n"
           "Options:\n"
           "  --headless            Simulate without a terminal, writing the census\n"
           "                        to stdout as CSV. For tuning and for CI.\n"
           "  --ticks N             Ticks to simulate headless (default 1000).\n"
           "  --census-every N      Emit a census row every N ticks (default 50).\n"
           "  --seed N              World and simulation seed (default 42). One seed\n"
           "                        reproduces a whole run.\n"
           "  -h, --help            Show this message.\n";
}

}
