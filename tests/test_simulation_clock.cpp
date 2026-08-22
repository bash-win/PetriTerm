#include <catch2/catch_test_macros.hpp>

#include "petriterm/engine/SimulationClock.hpp"

using petriterm::engine::SimulationClock;

TEST_CASE("a new clock runs at normal speed", "[clock]") {
    const SimulationClock clock;
    REQUIRE_FALSE(clock.isPaused());
    REQUIRE(clock.speedStepIndex() == SimulationClock::kNormalSpeedStepIndex);
    REQUIRE(clock.describeSpeed() == "1x");
    REQUIRE(clock.elapsedTickCount() == 0);
    REQUIRE_FALSE(clock.isFallingBehind());
}

TEST_CASE("the initial speed step is clamped into range", "[clock]") {
    REQUIRE(SimulationClock(-5).speedStepIndex() == 0);
    REQUIRE(SimulationClock(99).speedStepIndex() == SimulationClock::kSpeedStepCount - 1);
    REQUIRE(SimulationClock(0).speedStepIndex() == 0);
}

TEST_CASE("pausing reports zero ticks per second", "[clock]") {
    SimulationClock clock;
    const double runningRate = clock.playbackTicksPerSecond();
    REQUIRE(runningRate > 0.0);

    clock.setPaused(true);
    REQUIRE(clock.isPaused());
    REQUIRE(clock.playbackTicksPerSecond() == 0.0);
    REQUIRE(clock.describeSpeed() == "PAUSED");

    // Pausing must not lose the selected speed: unpausing resumes where it was.
    clock.setPaused(false);
    REQUIRE(clock.playbackTicksPerSecond() == runningRate);
}

TEST_CASE("toggling pause alternates", "[clock]") {
    SimulationClock clock;
    clock.togglePause();
    REQUIRE(clock.isPaused());
    clock.togglePause();
    REQUIRE_FALSE(clock.isPaused());
}

TEST_CASE("speed steps clamp at both ends without wrapping", "[clock]") {
    SimulationClock clock;

    SECTION("the fastest step is a ceiling, not a wrap to slowest") {
        for (int step = 0; step < 20; ++step) {
            clock.selectFasterSpeed();
        }
        REQUIRE(clock.speedStepIndex() == SimulationClock::kSpeedStepCount - 1);
        REQUIRE(clock.describeSpeed() == "8x");
    }

    SECTION("the slowest step is a floor, not a wrap to fastest") {
        for (int step = 0; step < 20; ++step) {
            clock.selectSlowerSpeed();
        }
        REQUIRE(clock.speedStepIndex() == 0);
        REQUIRE(clock.describeSpeed() == "0.5x");
    }
}

TEST_CASE("each speed step is faster than the one below it", "[clock]") {
    SimulationClock clock(0);
    double previousRate = clock.playbackTicksPerSecond();
    REQUIRE(previousRate > 0.0);
    for (int step = 1; step < SimulationClock::kSpeedStepCount; ++step) {
        clock.selectFasterSpeed();
        const double rate = clock.playbackTicksPerSecond();
        REQUIRE(rate > previousRate);
        previousRate = rate;
    }
}

TEST_CASE("the speed labels are distinct across every step", "[clock]") {
    // The HUD reads these, and a duplicate would make two speeds indistinguishable.
    SimulationClock clock(0);
    std::string_view previousLabel = clock.describeSpeed();
    for (int step = 1; step < SimulationClock::kSpeedStepCount; ++step) {
        clock.selectFasterSpeed();
        REQUIRE(clock.describeSpeed() != previousLabel);
        previousLabel = clock.describeSpeed();
    }
}

TEST_CASE("the tick duration does not depend on playback speed", "[clock]") {
    // The determinism guarantee: changing speed changes how OFTEN a tick runs,
    // never how much simulated time one tick represents. If this ever became
    // speed-dependent, a replay at 2x would diverge from the same run at 1x.
    SimulationClock slow(0);
    SimulationClock fast(SimulationClock::kSpeedStepCount - 1);
    REQUIRE(slow.playbackTicksPerSecond() != fast.playbackTicksPerSecond());
    REQUIRE(SimulationClock::kSecondsPerSimulationTick > 0.0);
    // The constant is static, so there is exactly one value for every clock and
    // every speed.
    REQUIRE(SimulationClock::kSecondsPerSimulationTick == 0.1);
}

TEST_CASE("a single tick step is consumed exactly once", "[clock]") {
    SimulationClock clock;
    REQUIRE_FALSE(clock.consumeSingleTickStep());

    clock.requestSingleTickStep();
    REQUIRE(clock.consumeSingleTickStep());
    REQUIRE_FALSE(clock.consumeSingleTickStep());
}

TEST_CASE("repeated step requests before a frame collapse into one", "[clock]") {
    // Holding the key must not bank steps that all fire on the next frame.
    SimulationClock clock;
    clock.requestSingleTickStep();
    clock.requestSingleTickStep();
    clock.requestSingleTickStep();
    REQUIRE(clock.consumeSingleTickStep());
    REQUIRE_FALSE(clock.consumeSingleTickStep());
}

TEST_CASE("a single tick step is available while paused", "[clock]") {
    // Stepping through a collapse while paused is the whole point of the feature.
    SimulationClock clock;
    clock.setPaused(true);
    clock.requestSingleTickStep();
    REQUIRE(clock.playbackTicksPerSecond() == 0.0);
    REQUIRE(clock.consumeSingleTickStep());
}

TEST_CASE("elapsed ticks accumulate and ignore non-positive counts", "[clock]") {
    SimulationClock clock;
    clock.recordTicksAdvanced(3);
    clock.recordTicksAdvanced(5);
    REQUIRE(clock.elapsedTickCount() == 8);

    // A frame that ran no ticks, which is most frames, must not move the counter
    // or underflow it.
    clock.recordTicksAdvanced(0);
    clock.recordTicksAdvanced(-4);
    REQUIRE(clock.elapsedTickCount() == 8);
}

TEST_CASE("the falling-behind flag latches then clears after a hold", "[clock]") {
    SimulationClock clock;
    REQUIRE_FALSE(clock.isFallingBehind());

    clock.recordFrameBacklogDropped(true);
    REQUIRE(clock.isFallingBehind());

    // It is held for a while so the HUD indicator does not flicker between
    // frames, but it does clear once the simulation keeps up.
    bool clearedWithinOneSecondOfFrames = false;
    for (int frame = 0; frame < 120; ++frame) {
        clock.recordFrameBacklogDropped(false);
        if (!clock.isFallingBehind()) {
            clearedWithinOneSecondOfFrames = true;
            break;
        }
    }
    REQUIRE(clearedWithinOneSecondOfFrames);
}

TEST_CASE("the falling-behind flag does not clear on the very next frame", "[clock]") {
    SimulationClock clock;
    clock.recordFrameBacklogDropped(true);
    clock.recordFrameBacklogDropped(false);
    REQUIRE(clock.isFallingBehind());
}

TEST_CASE("a repeated drop re-arms the falling-behind hold", "[clock]") {
    SimulationClock clock;
    clock.recordFrameBacklogDropped(true);
    for (int frame = 0; frame < 10; ++frame) {
        clock.recordFrameBacklogDropped(false);
    }
    REQUIRE(clock.isFallingBehind());
    clock.recordFrameBacklogDropped(true);
    for (int frame = 0; frame < 10; ++frame) {
        clock.recordFrameBacklogDropped(false);
    }
    REQUIRE(clock.isFallingBehind());
}
