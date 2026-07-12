#include "petriterm/engine/GameLoop.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

#include "petriterm/engine/InputManager.hpp"
#include "petriterm/engine/Renderer.hpp"
#include "petriterm/engine/SceneManager.hpp"

namespace petriterm::engine {

namespace {

using SecondsClock = std::chrono::steady_clock;

/// Upper bound on simulation ticks processed in a single frame, preventing a
/// slow tick from spiraling into an ever-growing backlog that starves
/// rendering.
constexpr int kMaximumTicksPerFrame = 8;

double secondsBetween(SecondsClock::time_point start, SecondsClock::time_point end) {
    return std::chrono::duration<double>(end - start).count();
}

}

GameLoop::GameLoop(int targetRenderFramesPerSecond, double initialSimulationTicksPerSecond)
    : targetRenderFramesPerSecond(targetRenderFramesPerSecond),
      simulationTicksPerSecond(initialSimulationTicksPerSecond) {}

void GameLoop::setSimulationTicksPerSecond(double ticksPerSecond) {
    simulationTicksPerSecond = std::max(0.0, ticksPerSecond);
}

void GameLoop::runUntilExitRequested(SceneManager& sceneManager, InputManager& inputManager,
                                     Renderer& renderer) {
    const double targetFrameSeconds =
        1.0 / static_cast<double>(targetRenderFramesPerSecond);
    auto previousFrameStart = SecondsClock::now();
    double accumulatedSeconds = 0.0;

    while (!sceneManager.exitRequested() && sceneManager.hasActiveScene()) {
        const auto frameStart = SecondsClock::now();
        const double elapsedSeconds = secondsBetween(previousFrameStart, frameStart);
        previousFrameStart = frameStart;

        inputManager.pollPendingKeyEvents();
        while (const std::optional<KeyEvent> event = inputManager.takeNextKeyEvent()) {
            sceneManager.dispatchKeyEvent(*event);
            if (sceneManager.exitRequested()) {
                return;
            }
        }

        if (simulationTicksPerSecond > 0.0) {
            const double tickDurationSeconds = 1.0 / simulationTicksPerSecond;
            accumulatedSeconds += elapsedSeconds;
            int ticksThisFrame = 0;
            while (accumulatedSeconds >= tickDurationSeconds &&
                   ticksThisFrame < kMaximumTicksPerFrame) {
                sceneManager.updateActiveScene(tickDurationSeconds);
                accumulatedSeconds -= tickDurationSeconds;
                ++ticksThisFrame;
            }
            if (accumulatedSeconds > tickDurationSeconds * kMaximumTicksPerFrame) {
                accumulatedSeconds = 0.0;
            }
        } else {
            accumulatedSeconds = 0.0;
        }

        sceneManager.renderActiveScene(renderer);

        const double frameWorkSeconds = secondsBetween(frameStart, SecondsClock::now());
        const double remainingSeconds = targetFrameSeconds - frameWorkSeconds;
        if (remainingSeconds > 0.0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(remainingSeconds));
        }
    }
}

}
