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
    /// Owns the resize path, because a resize is a property of the terminal
    /// rather than of whatever scene happens to be on top: the loop adopts the
    /// new size and lays out the whole scene stack for it before dispatching any
    /// further input, so no scene needs a case for it and every scene is handled
    /// the same way. Scenes are also laid out once before the first frame, which
    /// is what lets a scene take no dimensions at construction.
    ///
    /// While the terminal is below the minimum playable size the loop shows a
    /// resize notice instead of the scene stack, still polling input so the
    /// player can quit. The check runs every frame and again after every resize,
    /// so startup in a small terminal and shrinking one mid-game behave
    /// identically, and growing one back recovers.
    void runUntilExitRequested(SceneManager& sceneManager, InputManager& inputManager,
                               Renderer& renderer, TerminalWindow& terminal);

private:
    int targetRenderFramesPerSecond;
    SimulationClock& simulationClock;
};

}
