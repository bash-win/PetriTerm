#pragma once

#include <cstdint>
#include <string>

namespace petriterm::game {

/// What the process was asked to do. Deliberately small: this covers only the
/// flags a tuning run needs, because the full flag set, a config file, and the XDG
/// locations are a milestone of their own and building half of it here would mean
/// unpicking it later.
struct CommandLineOptions {
    /// Run the simulation without a terminal, writing the census to stdout as CSV.
    bool runHeadless = false;

    /// Ticks to simulate in a headless run. Ignored when playing normally, where
    /// the clock and the player decide.
    int ticksToRun = 1000;

    /// Emit a census row every this many ticks. A five-thousand-tick run at every
    /// tick is more output than is readable and no more informative.
    int censusIntervalInTicks = 50;

    std::uint64_t worldSeed = 42;

    bool showHelp = false;

    /// Empty when the arguments parsed cleanly. Set to a message naming the
    /// offending argument otherwise, in which case every other field is
    /// meaningless and the caller should print this and exit non-zero.
    std::string errorMessage;

    /// Returns true if parsing failed.
    bool hasError() const { return !errorMessage.empty(); }
};

/// Parses the process arguments, argv[0] included and ignored.
///
/// Reports errors in the returned struct rather than throwing or exiting, so the
/// whole of argument handling is a pure function over a vector of strings and can
/// be tested without spawning a process.
CommandLineOptions parseCommandLineOptions(int argumentCount,
                                           const char* const* argumentValues);

/// Returns the text for --help, ending in a newline.
std::string commandLineUsageText();

}
