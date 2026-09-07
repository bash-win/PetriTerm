#pragma once

#include <memory>

#include "petriterm/engine/ScreenRegion.hpp"

namespace petriterm::engine {

class Renderer;
struct KeyEvent;
class Scene;

/// What the scene stack should do after a scene handles a key event.
enum class TransitionKind {
    None,
    Push,
    Pop,
    Replace,
    Exit,
};

/// The result of handling a key event: a request to the SceneManager to keep
/// the current scene, push a new one, pop the current one, replace it, or exit
/// the application. sceneToPush is populated only for Push and Replace.
struct SceneTransition {
    TransitionKind kind = TransitionKind::None;
    std::unique_ptr<Scene> sceneToPush;

    /// Keeps the current scene unchanged.
    static SceneTransition stay() { return {TransitionKind::None, nullptr}; }

    /// Pushes a new scene on top of the current one.
    static SceneTransition push(std::unique_ptr<Scene> scene) {
        return {TransitionKind::Push, std::move(scene)};
    }

    /// Pops the current scene, returning to the one beneath it.
    static SceneTransition pop() { return {TransitionKind::Pop, nullptr}; }

    /// Replaces the current scene with a new one.
    static SceneTransition replace(std::unique_ptr<Scene> scene) {
        return {TransitionKind::Replace, std::move(scene)};
    }

    /// Requests that the application exit.
    static SceneTransition exitApplication() { return {TransitionKind::Exit, nullptr}; }
};

/// Abstract interface every screen implements. The SceneManager routes update,
/// render, and input to the scene on top of its stack.
class Scene {
public:
    Scene() = default;
    virtual ~Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = delete;
    Scene& operator=(Scene&&) = delete;

    /// Assigns the region of the terminal this scene draws into, and is the only
    /// place a scene is permitted to work out where its panes go.
    ///
    /// Pure virtual rather than a no-op default on purpose: a scene that ignores
    /// this is a scene whose layout is wrong the first time the player opens a
    /// tmux split, and that mistake is invisible until someone resizes. Making it
    /// a compile error to leave out means a scene with genuinely nothing to lay
    /// out says so in one empty override, rather than being indistinguishable
    /// from one that forgot.
    ///
    /// The SceneManager calls this before the scene's first update or render and
    /// again on every resize, so an implementation can treat it as the single
    /// source of its geometry and hold no dimensions from construction.
    virtual void relayout(const ScreenRegion& screenRegion) = 0;

    /// Advances this scene's state by the given fixed tick duration in seconds.
    virtual void update(double tickDeltaSeconds) = 0;

    /// Draws this scene using the given renderer.
    virtual void render(Renderer& renderer) = 0;

    /// Handles one key event and returns the requested scene-stack transition.
    virtual SceneTransition handleKeyEvent(const KeyEvent& event) = 0;
};

}
