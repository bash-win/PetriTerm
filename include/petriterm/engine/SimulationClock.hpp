#pragma once

#include <cstdint>
#include <string_view>

namespace petriterm::engine {

/// The single source of truth for simulation pacing. GameLoop reads it to decide
/// how many fixed ticks to run each frame; scenes write it to pause, change
/// speed, or single-step.
///
/// Kept separate from GameLoop for two reasons. First, GameLoop cannot be unit
/// tested - it sleeps and needs a live Renderer - so any pause and speed logic
/// living there would be untestable too, whereas this is a pure state machine.
/// Second, and more importantly, it keeps playback speed away from the tick
/// duration the simulation sees: speed changes how *often* a tick runs, never how
/// much simulated time a tick represents. Without that separation a replay at 2x
/// would diverge from the same replay at 1x.
class SimulationClock {
public:
    /// Simulated seconds one tick represents. A compile-time constant rather than
    /// a value derived from the playback rate, which is what makes a run at any
    /// speed produce an identical tick sequence.
    static constexpr double kSecondsPerSimulationTick = 0.1;

    /// The number of selectable speed steps, from slowest to fastest.
    static constexpr int kSpeedStepCount = 5;

    /// The speed step corresponding to 1x, used as the default.
    static constexpr int kNormalSpeedStepIndex = 1;

    /// Constructs a clock at the given speed step, clamped into range, running.
    explicit SimulationClock(int initialSpeedStepIndex = kNormalSpeedStepIndex);

    /// Ticks per real second at the current speed, or 0.0 while paused. GameLoop
    /// uses this for pacing only.
    double playbackTicksPerSecond() const;

    bool isPaused() const;
    void setPaused(bool paused);
    void togglePause();

    /// Moves one step up or down the speed ladder. Clamped at both ends rather
    /// than wrapping, so holding the key cannot silently jump from fastest to
    /// slowest.
    void selectFasterSpeed();
    void selectSlowerSpeed();
    int speedStepIndex() const;

    /// A HUD label for the current state: "PAUSED" when paused, otherwise the
    /// speed multiplier such as "1x".
    std::string_view describeSpeed() const;

    /// Queues exactly one tick to run on the next frame even while paused, so the
    /// player can single-step a collapse and read what changed between ticks.
    void requestSingleTickStep();

    /// Returns whether a single step was queued, clearing the request. Called by
    /// GameLoop once per frame.
    bool consumeSingleTickStep();

    /// Total ticks played back so far. This counts playback, not simulated
    /// history; once SimulationEngine exists it owns the authoritative tick index
    /// that a save file records.
    std::uint64_t elapsedTickCount() const;
    void recordTicksAdvanced(int tickCount);

    /// True while the loop has recently been discarding accumulated time to
    /// protect the frame rate, so the HUD can warn that the simulation is running
    /// slower than the selected speed. Held briefly after the last drop so the
    /// indicator does not flicker on and off between frames.
    bool isFallingBehind() const;
    void recordFrameBacklogDropped(bool backlogWasDropped);

private:
    int currentSpeedStepIndex;
    bool paused = false;
    bool singleTickStepRequested = false;
    std::uint64_t playedBackTickCount = 0;
    int fallingBehindHoldFrames = 0;
};

}
