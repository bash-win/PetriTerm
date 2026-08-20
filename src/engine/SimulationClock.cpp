#include "petriterm/engine/SimulationClock.hpp"

#include <algorithm>
#include <array>

namespace petriterm::engine {

namespace {

/// Ticks per second at 1x. Chosen so a rabbit's 120-tick lifespan lasts about
/// twelve seconds: fast enough to watch a population turn over, slow enough to
/// read what happened. The previous 30 gave it four seconds.
constexpr double kBaseTicksPerSecond = 10.0;

/// The selectable speed multipliers, slowest first.
constexpr std::array<double, SimulationClock::kSpeedStepCount> kSpeedMultipliers{
    0.5, 1.0, 2.0, 4.0, 8.0};

/// HUD labels parallel to kSpeedMultipliers.
constexpr std::array<std::string_view, SimulationClock::kSpeedStepCount> kSpeedLabels{
    "0.5x", "1x", "2x", "4x", "8x"};

/// Frames the falling-behind indicator stays lit after the last dropped backlog.
/// At 30 frames per second this holds it for about one second, long enough to
/// read and short enough to clear promptly once the simulation catches up.
constexpr int kFallingBehindHoldFrameCount = 30;

}

SimulationClock::SimulationClock(int initialSpeedStepIndex)
    : currentSpeedStepIndex(std::clamp(initialSpeedStepIndex, 0, kSpeedStepCount - 1)) {}

double SimulationClock::playbackTicksPerSecond() const {
    if (paused) {
        return 0.0;
    }
    return kBaseTicksPerSecond *
           kSpeedMultipliers[static_cast<std::size_t>(currentSpeedStepIndex)];
}

bool SimulationClock::isPaused() const {
    return paused;
}

void SimulationClock::setPaused(bool paused) {
    this->paused = paused;
}

void SimulationClock::togglePause() {
    paused = !paused;
}

void SimulationClock::selectFasterSpeed() {
    currentSpeedStepIndex = std::min(currentSpeedStepIndex + 1, kSpeedStepCount - 1);
}

void SimulationClock::selectSlowerSpeed() {
    currentSpeedStepIndex = std::max(currentSpeedStepIndex - 1, 0);
}

int SimulationClock::speedStepIndex() const {
    return currentSpeedStepIndex;
}

std::string_view SimulationClock::describeSpeed() const {
    if (paused) {
        return "PAUSED";
    }
    return kSpeedLabels[static_cast<std::size_t>(currentSpeedStepIndex)];
}

void SimulationClock::requestSingleTickStep() {
    singleTickStepRequested = true;
}

bool SimulationClock::consumeSingleTickStep() {
    const bool wasRequested = singleTickStepRequested;
    singleTickStepRequested = false;
    return wasRequested;
}

std::uint64_t SimulationClock::elapsedTickCount() const {
    return playedBackTickCount;
}

void SimulationClock::recordTicksAdvanced(int tickCount) {
    if (tickCount <= 0) {
        return;
    }
    playedBackTickCount += static_cast<std::uint64_t>(tickCount);
}

bool SimulationClock::isFallingBehind() const {
    return fallingBehindHoldFrames > 0;
}

void SimulationClock::recordFrameBacklogDropped(bool backlogWasDropped) {
    if (backlogWasDropped) {
        fallingBehindHoldFrames = kFallingBehindHoldFrameCount;
        return;
    }
    fallingBehindHoldFrames = std::max(fallingBehindHoldFrames - 1, 0);
}

}
