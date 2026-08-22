#pragma once

namespace petriterm::engine {

class SceneManager;
class InputManager;
class Renderer;
class TerminalWindow;
class SimulationClock;

/// Owns timing. Runs a fixed-timestep simulation decoupled from variable-rate
/// rendering: the simulation advances in ticks of a constant simulated duration
/// while frames are drawn at the render rate, so the tick sequence is identical
/// no matter how fast the frames are or what playback speed the player chose.
class GameLoop {
public:
    /// Constructs a loop targeting the given render frame rate and driven by the
    /// given clock. The clock is owned externally and shared with the scenes that
    /// pause and re-speed it.
    GameLoop(int targetRenderFramesPerSecond, SimulationClock& simulationClock);

    /// Runs until the active scene requests exit or the scene stack empties.
    /// Each iteration polls input, advances the simulation by as many fixed
    /// ticks as elapsed time allows, renders one frame, and sleeps to hold the
    /// target frame rate.
    ///
    /// While the terminal is below the minimum playable size the loop shows a
    /// resize notice instead of the scene stack, still polling input so the
    /// player can quit. The check runs every frame, so startup in a small
    /// terminal and shrinking one mid-game behave identically.
    void runUntilExitRequested(SceneManager& sceneManager, InputManager& inputManager,
                               Renderer& renderer, const TerminalWindow& terminal);

private:
    int targetRenderFramesPerSecond;
    SimulationClock& simulationClock;
};

}
