#include "petriterm/engine/GameLoop.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <thread>

#include "petriterm/engine/InputManager.hpp"
#include "petriterm/engine/Renderer.hpp"
#include "petriterm/engine/SceneManager.hpp"
#include "petriterm/engine/SimulationClock.hpp"
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

GameLoop::GameLoop(int targetRenderFramesPerSecond, SimulationClock& simulationClock)
    : targetRenderFramesPerSecond(std::max(1, targetRenderFramesPerSecond)),
      simulationClock(simulationClock) {}

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

        // Every tick is advanced by the same constant simulated duration. The
        // playback rate below decides only how many ticks run this frame, never
        // how much time one tick represents - that separation is what keeps a
        // run at 8x identical to the same run at 1x.
        int ticksThisFrame = 0;
        if (simulationClock.consumeSingleTickStep()) {
            sceneManager.updateActiveScene(SimulationClock::kSecondsPerSimulationTick);
            ticksThisFrame = 1;
            accumulatedSeconds = 0.0;
            simulationClock.recordFrameBacklogDropped(false);
        } else if (const double ticksPerSecond = simulationClock.playbackTicksPerSecond();
                   ticksPerSecond > 0.0) {
            const double tickPeriodSeconds = 1.0 / ticksPerSecond;
            const double backlogCapSeconds = tickPeriodSeconds * kMaximumTicksPerFrame;
            const double requestedSeconds = accumulatedSeconds + elapsedSeconds;
            const bool backlogWasDropped = requestedSeconds > backlogCapSeconds;
            accumulatedSeconds = std::min(requestedSeconds, backlogCapSeconds);
            while (accumulatedSeconds >= tickPeriodSeconds) {
                sceneManager.updateActiveScene(SimulationClock::kSecondsPerSimulationTick);
                accumulatedSeconds -= tickPeriodSeconds;
                ++ticksThisFrame;
            }
            simulationClock.recordFrameBacklogDropped(backlogWasDropped);
        } else {
            // Paused: drop banked time so unpausing does not release a burst of
            // catch-up ticks.
            accumulatedSeconds = 0.0;
            simulationClock.recordFrameBacklogDropped(false);
        }
        simulationClock.recordTicksAdvanced(ticksThisFrame);

        sceneManager.renderActiveScene(renderer);
        sleepRemainderOfFrame(frameStart);
    }
}

}
