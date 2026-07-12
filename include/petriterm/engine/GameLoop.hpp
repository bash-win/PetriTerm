#pragma once

namespace petriterm::engine {

class SceneManager;
class InputManager;
class Renderer;

/// Owns timing. Runs a fixed-timestep simulation decoupled from variable-rate
/// rendering: simulation ticks advance at a player-adjustable rate independent
/// of the render frame rate, so the simulation stays deterministic regardless
/// of how fast frames are drawn.
class GameLoop {
public:
    /// Constructs a loop targeting the given render frame rate and the given
    /// initial simulation tick rate (ticks per second; 0 pauses simulation).
    GameLoop(int targetRenderFramesPerSecond, double initialSimulationTicksPerSecond);

    /// Runs until the active scene requests exit or the scene stack empties.
    /// Each iteration polls input, advances the simulation by as many fixed
    /// ticks as elapsed time allows, renders one frame, and sleeps to hold the
    /// target frame rate.
    void runUntilExitRequested(SceneManager& sceneManager, InputManager& inputManager,
                               Renderer& renderer);

    /// Sets how many simulation ticks occur per real second. Zero pauses the
    /// simulation while rendering continues.
    void setSimulationTicksPerSecond(double ticksPerSecond);

private:
    int targetRenderFramesPerSecond;
    double simulationTicksPerSecond;
};

}
