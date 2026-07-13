#include "petriterm/engine/SceneManager.hpp"

#include <utility>

namespace petriterm::engine {

void SceneManager::pushScene(std::unique_ptr<Scene> scene) {
    if (scene) {
        sceneStack.push_back(std::move(scene));
    }
}

bool SceneManager::hasActiveScene() const {
    return !sceneStack.empty();
}

bool SceneManager::exitRequested() const {
    return exitHasBeenRequested;
}

void SceneManager::updateActiveScene(double tickDeltaSeconds) {
    if (!sceneStack.empty()) {
        sceneStack.back()->update(tickDeltaSeconds);
    }
}

void SceneManager::renderActiveScene(Renderer& renderer) {
    if (!sceneStack.empty()) {
        sceneStack.back()->render(renderer);
    }
}

void SceneManager::dispatchKeyEvent(const KeyEvent& event) {
    if (sceneStack.empty()) {
        return;
    }
    applyTransition(sceneStack.back()->handleKeyEvent(event));
}

void SceneManager::applyTransition(SceneTransition transition) {
    switch (transition.kind) {
        case TransitionKind::None:
            break;
        case TransitionKind::Push:
            if (transition.sceneToPush) {
                sceneStack.push_back(std::move(transition.sceneToPush));
            }
            break;
        case TransitionKind::Pop:
            if (!sceneStack.empty()) {
                sceneStack.pop_back();
            }
            break;
        case TransitionKind::Replace:
            if (!sceneStack.empty()) {
                sceneStack.pop_back();
            }
            if (transition.sceneToPush) {
                sceneStack.push_back(std::move(transition.sceneToPush));
            }
            break;
        case TransitionKind::Exit:
            exitHasBeenRequested = true;
            break;
    }
}

}
