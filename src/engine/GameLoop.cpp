#include "petriterm/engine/GameLoop.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <thread>

#include "petriterm/engine/InputManager.hpp"
#include "petriterm/engine/Renderer.hpp"
#include "petriterm/engine/SceneManager.hpp"
#include "petriterm/engine/TerminalWindow.hpp"

namespace petriterm::engine {

namespace {

using SecondsClock = std::chrono::steady_clock;

/// Upper bound on simulation ticks processed in a single frame. Accumulated
/// time beyond this many ticks is discarded, preventing a slow tick from
/// spiraling into an ever-growing backlog that starves rendering.
constexpr int kMaximumTicksPerFrame = 8;

/// The smallest terminal the game's layout is designed for. Below this the loop
/// shows a resize notice rather than letting scenes draw into a space they cannot
/// lay out in.
constexpr int kMinimumTerminalColumns = 80;
constexpr int kMinimumTerminalRows = 24;

double secondsBetween(SecondsClock::time_point start, SecondsClock::time_point end) {
    return std::chrono::duration<double>(end - start).count();
}

/// Draws the resize prompt shown while the terminal is too small. Drawn through
/// the Renderer rather than raw ncurses so it uses the same styling path as the
/// rest of the UI, and redrawn every frame so it tracks a live resize.
void renderTerminalTooSmallNotice(Renderer& renderer, TerminalDimensions dimensions) {
    renderer.beginFrame();
    renderer.drawText(
        0, 0, "Terminal too small.",
        TextStyle{TerminalColor::Yellow, TerminalColor::Default, TextEmphasis::Bold});
    renderer.drawText(0, 1,
                      std::format("Need at least {} x {}; current size is {} x {}.",
                                  kMinimumTerminalColumns, kMinimumTerminalRows,
                                  dimensions.columns, dimensions.rows));
    renderer.drawText(0, 2, "Resize the terminal, or press q to quit.",
                      TextStyle{TerminalColor::Cyan});
    renderer.endFrame();
}

/// Returns true if the key event is the quit key the resize notice advertises.
bool isQuitKey(const KeyEvent& event) {
    return event.code == KeyCode::Character &&
           (event.character == L'q' || event.character == L'Q');
}

}

GameLoop::GameLoop(int targetRenderFramesPerSecond, double initialSimulationTicksPerSecond)
    : targetRenderFramesPerSecond(std::max(1, targetRenderFramesPerSecond)),
      simulationTicksPerSecond(std::max(0.0, initialSimulationTicksPerSecond)) {}

void GameLoop::setSimulationTicksPerSecond(double ticksPerSecond) {
    simulationTicksPerSecond = std::max(0.0, ticksPerSecond);
}

void GameLoop::runUntilExitRequested(SceneManager& sceneManager, InputManager& inputManager,
                                     Renderer& renderer, const TerminalWindow& terminal) {
    const double targetFrameSeconds =
        1.0 / static_cast<double>(targetRenderFramesPerSecond);
    auto previousFrameStart = SecondsClock::now();
    double accumulatedSeconds = 0.0;

    const auto sleepRemainderOfFrame = [targetFrameSeconds](
                                           SecondsClock::time_point frameStart) {
        const double frameWorkSeconds = secondsBetween(frameStart, SecondsClock::now());
        const double remainingSeconds = targetFrameSeconds - frameWorkSeconds;
        if (remainingSeconds > 0.0) {
            std::this_thread::sleep_for(std::chrono::duration<double>(remainingSeconds));
        }
    };

    while (!sceneManager.exitRequested() && sceneManager.hasActiveScene()) {
        const auto frameStart = SecondsClock::now();
        const double elapsedSeconds = secondsBetween(previousFrameStart, frameStart);
        previousFrameStart = frameStart;

        const TerminalDimensions dimensions = terminal.currentDimensions();
        const bool terminalIsPlayable = dimensions.columns >= kMinimumTerminalColumns &&
                                        dimensions.rows >= kMinimumTerminalRows;

        inputManager.pollPendingKeyEvents();
        while (const std::optional<KeyEvent> event = inputManager.takeNextKeyEvent()) {
            if (!terminalIsPlayable) {
                // Scenes cannot lay out at this size, so honor only the quit key
                // and drop the rest rather than delivering input the player
                // cannot see the effect of.
                if (isQuitKey(*event)) {
                    return;
                }
                continue;
            }
            sceneManager.dispatchKeyEvent(*event);
            if (sceneManager.exitRequested()) {
                return;
            }
        }

        if (!terminalIsPlayable) {
            renderTerminalTooSmallNotice(renderer, dimensions);
            // Discard banked time so resizing does not release a burst of
            // catch-up ticks the moment the terminal becomes playable again.
            accumulatedSeconds = 0.0;
            sleepRemainderOfFrame(frameStart);
            continue;
        }

        if (simulationTicksPerSecond > 0.0) {
            const double tickDurationSeconds = 1.0 / simulationTicksPerSecond;
            accumulatedSeconds = std::min(accumulatedSeconds + elapsedSeconds,
                                          tickDurationSeconds * kMaximumTicksPerFrame);
            while (accumulatedSeconds >= tickDurationSeconds) {
                sceneManager.updateActiveScene(tickDurationSeconds);
                accumulatedSeconds -= tickDurationSeconds;
            }
        } else {
            accumulatedSeconds = 0.0;
        }

        sceneManager.renderActiveScene(renderer);
        sleepRemainderOfFrame(frameStart);
    }
}

}
