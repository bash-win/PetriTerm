#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "petriterm/engine/Scene.hpp"
#include "petriterm/engine/ScreenRegion.hpp"

namespace petriterm::engine {

class Renderer;
struct KeyEvent;

/// Owns a stack of scenes and routes update, render, and input to the top
/// scene, applying the transition each scene requests. Enables pause overlays
/// and menu -> game -> game-over flows.
class SceneManager {
public:
    /// Pushes a scene onto the stack, making it the active scene. If a screen
    /// region has been set, the new scene is laid out for it before it can be
    /// updated or rendered, so a scene pushed mid-game never has to be told the
    /// terminal size by whoever pushed it.
    void pushScene(std::unique_ptr<Scene> scene);

    /// Records a new drawable screen region and lays out every scene on the
    /// stack for it, not only the active one.
    ///
    /// The whole stack, because a scene underneath an overlay is still live: it
    /// keeps updating, it is what the player returns to when the overlay pops,
    /// and if it were laid out lazily on becoming active again it would render
    /// one frame at the old size. Laying out a covered scene costs a resize
    /// event's worth of arithmetic, which is not worth optimizing away.
    void relayoutAllScenes(const ScreenRegion& screenRegion);

    /// The screen region scenes are currently laid out for, or std::nullopt
    /// before the first relayoutAllScenes call. Exposed so the caller driving
    /// resizes can skip propagating a size that has not actually changed.
    const std::optional<ScreenRegion>& currentScreenRegion() const;

    /// Returns true while at least one scene remains on the stack.
    bool hasActiveScene() const;

    /// Returns true once a scene has requested application exit.
    bool exitRequested() const;

    /// Advances the active scene by the given fixed tick duration; no-op if the
    /// stack is empty.
    void updateActiveScene(double tickDeltaSeconds);

    /// Renders the active scene; no-op if the stack is empty.
    void renderActiveScene(Renderer& renderer);

    /// Forwards a key event to the active scene and applies the transition it
    /// returns; no-op if the stack is empty.
    void dispatchKeyEvent(const KeyEvent& event);

private:
    void applyTransition(SceneTransition transition);
    void adoptScene(std::unique_ptr<Scene> scene);

    std::vector<std::unique_ptr<Scene>> sceneStack;
    std::optional<ScreenRegion> screenRegion;
    bool exitHasBeenRequested = false;
};

}
