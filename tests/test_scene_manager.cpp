#include <memory>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "petriterm/engine/InputManager.hpp"
#include "petriterm/engine/Scene.hpp"
#include "petriterm/engine/SceneManager.hpp"
#include "petriterm/engine/ScreenRegion.hpp"

using petriterm::engine::KeyCode;
using petriterm::engine::KeyEvent;
using petriterm::engine::Scene;
using petriterm::engine::SceneManager;
using petriterm::engine::SceneTransition;
using petriterm::engine::ScreenRegion;

namespace {

/// A scene that records every relayout and update it is given and requests a
/// transition chosen by the test.
///
/// Rendering is the one thing it cannot record, because a Renderer needs a live
/// ncurses window. That is also the only thing SceneManager does not decide - it
/// forwards render to the top scene and nothing else - so leaving it out costs
/// no coverage of what this file is testing.
class RecordingScene : public Scene {
public:
    explicit RecordingScene(SceneTransition (*transitionForKey)() = nullptr)
        : transitionForKey(transitionForKey) {}

    void relayout(const ScreenRegion& screenRegion) override {
        relayoutRegions.push_back(screenRegion);
    }

    void update(double tickDeltaSeconds) override {
        updateCallCount += 1;
        lastTickDeltaSeconds = tickDeltaSeconds;
    }

    void render(petriterm::engine::Renderer&) override { renderCallCount += 1; }

    SceneTransition handleKeyEvent(const KeyEvent&) override {
        keyEventCount += 1;
        return transitionForKey == nullptr ? SceneTransition::stay() : transitionForKey();
    }

    /// The region of the most recent relayout, or std::nullopt if there has not
    /// been one. The distinction is the point of most of these tests.
    std::optional<ScreenRegion> mostRecentRelayoutRegion() const {
        if (relayoutRegions.empty()) {
            return std::nullopt;
        }
        return relayoutRegions.back();
    }

    std::vector<ScreenRegion> relayoutRegions;
    int updateCallCount = 0;
    int renderCallCount = 0;
    int keyEventCount = 0;
    double lastTickDeltaSeconds = 0.0;

private:
    SceneTransition (*transitionForKey)();
};

constexpr ScreenRegion kStandardScreen{0, 0, 80, 24};
constexpr ScreenRegion kWideScreen{0, 0, 160, 48};

const KeyEvent kSpaceKey{KeyCode::Space, L'\0', 0};

}

TEST_CASE("a scene manager has no screen region before the first relayout",
          "[scene-manager]") {
    SceneManager sceneManager;
    REQUIRE_FALSE(sceneManager.currentScreenRegion().has_value());

    auto scene = std::make_unique<RecordingScene>();
    const RecordingScene* scenePointer = scene.get();
    sceneManager.pushScene(std::move(scene));
    REQUIRE_FALSE(scenePointer->mostRecentRelayoutRegion().has_value());
}

TEST_CASE("relayoutAllScenes records the region and forwards it", "[scene-manager]") {
    SceneManager sceneManager;
    auto scene = std::make_unique<RecordingScene>();
    const RecordingScene* scenePointer = scene.get();
    sceneManager.pushScene(std::move(scene));

    sceneManager.relayoutAllScenes(kStandardScreen);
    REQUIRE(sceneManager.currentScreenRegion() == kStandardScreen);
    REQUIRE(scenePointer->mostRecentRelayoutRegion() == kStandardScreen);

    sceneManager.relayoutAllScenes(kWideScreen);
    REQUIRE(sceneManager.currentScreenRegion() == kWideScreen);
    REQUIRE(scenePointer->mostRecentRelayoutRegion() == kWideScreen);
    REQUIRE(scenePointer->relayoutRegions.size() == 2);
}

TEST_CASE("relayoutAllScenes reaches scenes underneath the active one", "[scene-manager]") {
    SceneManager sceneManager;
    auto coveredScene = std::make_unique<RecordingScene>();
    auto overlayScene = std::make_unique<RecordingScene>();
    const RecordingScene* coveredPointer = coveredScene.get();
    const RecordingScene* overlayPointer = overlayScene.get();
    sceneManager.pushScene(std::move(coveredScene));
    sceneManager.pushScene(std::move(overlayScene));

    sceneManager.relayoutAllScenes(kWideScreen);

    // The covered scene is what the player returns to when the overlay pops. If
    // only the active scene were laid out it would draw one frame at the old
    // size, which is the bug this behavior exists to prevent.
    REQUIRE(coveredPointer->mostRecentRelayoutRegion() == kWideScreen);
    REQUIRE(overlayPointer->mostRecentRelayoutRegion() == kWideScreen);
}

TEST_CASE("a scene pushed after a relayout is laid out before it is used",
          "[scene-manager]") {
    SceneManager sceneManager;
    sceneManager.relayoutAllScenes(kStandardScreen);

    auto scene = std::make_unique<RecordingScene>();
    const RecordingScene* scenePointer = scene.get();
    sceneManager.pushScene(std::move(scene));

    REQUIRE(scenePointer->mostRecentRelayoutRegion() == kStandardScreen);
    REQUIRE(scenePointer->relayoutRegions.size() == 1);
    REQUIRE(scenePointer->updateCallCount == 0);
}

TEST_CASE("a scene pushed by a transition is laid out too", "[scene-manager]") {
    SceneManager sceneManager;
    sceneManager.relayoutAllScenes(kStandardScreen);
    sceneManager.pushScene(std::make_unique<RecordingScene>(
        [] { return SceneTransition::push(std::make_unique<RecordingScene>()); }));

    sceneManager.dispatchKeyEvent(kSpaceKey);

    // Whoever built that transition never saw a terminal size, which is the
    // reason the manager rather than the pusher owns this.
    sceneManager.updateActiveScene(0.25);
    sceneManager.relayoutAllScenes(kWideScreen);
    REQUIRE(sceneManager.currentScreenRegion() == kWideScreen);
}

TEST_CASE("a scene installed by a replace transition is laid out", "[scene-manager]") {
    SceneManager sceneManager;
    sceneManager.relayoutAllScenes(kStandardScreen);
    sceneManager.pushScene(std::make_unique<RecordingScene>(
        [] { return SceneTransition::replace(std::make_unique<RecordingScene>()); }));

    sceneManager.dispatchKeyEvent(kSpaceKey);
    REQUIRE(sceneManager.hasActiveScene());

    sceneManager.updateActiveScene(0.25);
    sceneManager.relayoutAllScenes(kWideScreen);
    REQUIRE(sceneManager.currentScreenRegion() == kWideScreen);
}

TEST_CASE("relayoutAllScenes on an empty stack still records the region",
          "[scene-manager]") {
    SceneManager sceneManager;
    sceneManager.relayoutAllScenes(kStandardScreen);
    REQUIRE(sceneManager.currentScreenRegion() == kStandardScreen);
    REQUIRE_FALSE(sceneManager.hasActiveScene());
}

TEST_CASE("an empty screen region propagates rather than being suppressed",
          "[scene-manager]") {
    SceneManager sceneManager;
    auto scene = std::make_unique<RecordingScene>();
    const RecordingScene* scenePointer = scene.get();
    sceneManager.pushScene(std::move(scene));

    // A terminal can legitimately be reported as zero-sized mid-resize, and a
    // scene that is told so can decline to draw. Withholding it would leave the
    // scene laying out against a size that no longer exists.
    constexpr ScreenRegion collapsed{0, 0, 0, 0};
    sceneManager.relayoutAllScenes(collapsed);
    REQUIRE(scenePointer->mostRecentRelayoutRegion() == collapsed);
}

TEST_CASE("update, input, and exit still route to the active scene only",
          "[scene-manager]") {
    SceneManager sceneManager;
    auto coveredScene = std::make_unique<RecordingScene>();
    auto overlayScene = std::make_unique<RecordingScene>();
    const RecordingScene* coveredPointer = coveredScene.get();
    const RecordingScene* overlayPointer = overlayScene.get();
    sceneManager.pushScene(std::move(coveredScene));
    sceneManager.pushScene(std::move(overlayScene));

    sceneManager.updateActiveScene(0.5);
    sceneManager.dispatchKeyEvent(kSpaceKey);

    REQUIRE(overlayPointer->updateCallCount == 1);
    REQUIRE(overlayPointer->lastTickDeltaSeconds == 0.5);
    REQUIRE(overlayPointer->keyEventCount == 1);
    REQUIRE(coveredPointer->updateCallCount == 0);
    REQUIRE(coveredPointer->keyEventCount == 0);
    REQUIRE_FALSE(sceneManager.exitRequested());
}
