#include <algorithm>
#include <clocale>
#include <cstdio>
#include <exception>
#include <memory>
#include <string_view>

#include <ncurses.h>

#include "petriterm/engine/ColorPalette.hpp"
#include "petriterm/engine/GameLoop.hpp"
#include "petriterm/engine/InputManager.hpp"
#include "petriterm/engine/Renderer.hpp"
#include "petriterm/engine/Scene.hpp"
#include "petriterm/engine/SceneManager.hpp"
#include "petriterm/engine/TerminalWindow.hpp"

namespace {

using namespace petriterm::engine;

/// Bootstrap scene that moves an '@' with the arrow keys, proving the game
/// loop, input, and renderer integrate. Replaced by the real menu and
/// simulation screens in a later milestone.
class ArrowKeyDemoScene : public Scene {
public:
    void update(double) override {}

    void render(Renderer& renderer) override {
        constexpr std::string_view hint = "arrows: move the @    q / Esc: quit";
        renderer.beginFrame();
        renderer.drawText(2, 1, hint, TerminalColor::Cyan);
        renderer.drawGlyph(glyphColumn, glyphRow, L'@', TerminalColor::Yellow,
                           TerminalColor::Default, A_BOLD);
        renderer.endFrame();
    }

    SceneTransition handleKeyEvent(const KeyEvent& event) override {
        switch (event.code) {
            case KeyCode::ArrowUp:
                glyphRow = std::max(glyphRow - 1, 0);
                break;
            case KeyCode::ArrowDown:
                ++glyphRow;
                break;
            case KeyCode::ArrowLeft:
                glyphColumn = std::max(glyphColumn - 1, 0);
                break;
            case KeyCode::ArrowRight:
                ++glyphColumn;
                break;
            case KeyCode::Escape:
                return SceneTransition::exitApplication();
            case KeyCode::Character:
                if (event.character == L'q' || event.character == L'Q') {
                    return SceneTransition::exitApplication();
                }
                break;
            default:
                break;
        }
        return SceneTransition::stay();
    }

private:
    int glyphColumn = 10;
    int glyphRow = 5;
};

}

int main() {
    std::setlocale(LC_ALL, "");
    try {
        petriterm::engine::TerminalWindow terminal;
        if (!terminal.waitUntilTerminalIsAtLeast(80, 24)) {
            return 0;
        }
        petriterm::engine::ColorPalette palette;
        palette.initializeColorPairs();
        petriterm::engine::Renderer renderer(stdscr, palette);
        petriterm::engine::InputManager inputManager;
        petriterm::engine::SceneManager sceneManager;
        sceneManager.pushScene(std::make_unique<ArrowKeyDemoScene>());
        petriterm::engine::GameLoop gameLoop(60, 30.0);
        gameLoop.runUntilExitRequested(sceneManager, inputManager, renderer);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "PetriTerm fatal error: %s\n", error.what());
        return 1;
    }
    return 0;
}
