#pragma once

#include <memory>
#include <vector>

#include "petriterm/engine/Scene.hpp"

namespace petriterm::engine {

class Renderer;
struct KeyEvent;

/// Owns a stack of scenes and routes update, render, and input to the top
/// scene, applying the transition each scene requests. Enables pause overlays
/// and menu -> game -> game-over flows.
class SceneManager {
public:
    /// Pushes a scene onto the stack, making it the active scene.
    void pushScene(std::unique_ptr<Scene> scene);

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

    std::vector<std::unique_ptr<Scene>> sceneStack;
    bool exitHasBeenRequested = false;
};

}
