#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "petriterm/game/StarterEcosystem.hpp"
#include "petriterm/organisms/Species.hpp"
#include "petriterm/simulation/EcosystemCensus.hpp"

namespace petriterm::game {

/// What to simulate. One seed drives world generation, the starter placement, and
/// every choice the engine makes afterwards, so these settings alone determine the
/// entire run.
struct HeadlessRunSettings {
    std::uint64_t worldSeed = 42;
    int ticksToRun = 1000;
    int censusIntervalInTicks = 50;
    int worldWidthInTiles = 0;
    int worldHeightInTiles = 0;
    StarterDensityPerHundredTiles density;
};

/// Generates a world, seeds a starter ecosystem into it, and advances it for the
/// configured number of ticks, handing every sampled census to the callback.
///
/// Takes a callback rather than writing to stdout so the whole run is testable:
/// the soak test collects the rows and asserts on them, and only the command-line
/// path turns them into text. Nothing here touches a terminal.
///
/// A census is sampled on the last tick regardless of the interval, so a run's
/// final state is always reported and a test never has to pick a tick count
/// divisible by the interval to see where it ended up.
void runHeadlessSimulation(
    const HeadlessRunSettings& settings,
    const std::vector<const organisms::Species*>& palette,
    const std::function<void(const simulation::EcosystemCensus&)>& onCensusSampled);

}
