#include <clocale>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string_view>
#include <utility>

#include <ncurses.h>

#include "petriterm/engine/ColorPalette.hpp"
#include "petriterm/engine/GameLoop.hpp"
#include "petriterm/engine/InputManager.hpp"
#include "petriterm/engine/Renderer.hpp"
#include "petriterm/engine/Scene.hpp"
#include "petriterm/engine/SceneManager.hpp"
#include "petriterm/engine/TerminalWindow.hpp"
#include "petriterm/game/Viewport.hpp"
#include "petriterm/world/Biome.hpp"
#include "petriterm/world/WorldGenerator.hpp"
#include "petriterm/world/WorldGrid.hpp"

namespace {

using namespace petriterm::engine;
using petriterm::game::ScreenCell;
using petriterm::game::Viewport;
using petriterm::world::BiomeDescriptor;
using petriterm::world::describeBiome;
using petriterm::world::generateWorld;
using petriterm::world::Tile;
using petriterm::world::WorldGrid;

constexpr int kMinimumTerminalColumns = 80;
constexpr int kMinimumTerminalRows = 24;
constexpr std::uint64_t kBootstrapWorldSeed = 42;

/// Bootstrap scene that renders a generated world and lets the arrow keys scroll
/// the camera across it, proving world generation and the viewport integrate.
/// Replaced by the real simulation screen in a later milestone.
class WorldViewScene : public Scene {
public:
    WorldViewScene(WorldGrid generatedWorld, int screenColumns, int screenRows)
        : world(std::move(generatedWorld)),
          helpBarRow(screenRows - 1),
          viewport(world.widthInTiles(), world.heightInTiles(), 0, 0, screenColumns,
                   screenRows - 1) {}

    void update(double) override {}

    void render(Renderer& renderer) override {
        constexpr std::string_view hint = "arrows: scroll    q / Esc: quit    (seed 42)";
        renderer.beginFrame();
        for (int rowOffset = 0; rowOffset < viewport.visibleHeightInTiles(); ++rowOffset) {
            for (int columnOffset = 0; columnOffset < viewport.visibleWidthInTiles();
                 ++columnOffset) {
                const int tileColumn = viewport.cameraColumnIndex() + columnOffset;
                const int tileRow = viewport.cameraRowIndex() + rowOffset;
                const std::optional<ScreenCell> cell =
                    viewport.tileToScreenCell(tileColumn, tileRow);
                if (!cell) {
                    continue;
                }
                const Tile& tile = world.tileAt(tileColumn, tileRow);
                const BiomeDescriptor& descriptor = describeBiome(tile.biome);
                renderer.drawGlyph(cell->columnIndex, cell->rowIndex,
                                   descriptor.backgroundGlyph, TerminalColor::Black,
                                   descriptor.backgroundColor);
            }
        }
        renderer.drawText(0, helpBarRow, hint, TerminalColor::Cyan);
        renderer.endFrame();
    }

    SceneTransition handleKeyEvent(const KeyEvent& event) override {
        switch (event.code) {
            case KeyCode::ArrowUp:
                viewport.scrollByTiles(0, -1);
                break;
            case KeyCode::ArrowDown:
                viewport.scrollByTiles(0, 1);
                break;
            case KeyCode::ArrowLeft:
                viewport.scrollByTiles(-1, 0);
                break;
            case KeyCode::ArrowRight:
                viewport.scrollByTiles(1, 0);
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
    WorldGrid world;
    int helpBarRow;
    Viewport viewport;
};

}

int main() {
    std::setlocale(LC_ALL, "");
    try {
        petriterm::engine::TerminalWindow terminal;
        if (!terminal.waitUntilTerminalIsAtLeast(kMinimumTerminalColumns,
                                                 kMinimumTerminalRows)) {
            return 0;
        }
        petriterm::engine::ColorPalette palette;
        palette.initializeColorPairs();
        petriterm::engine::Renderer renderer(stdscr, palette);
        petriterm::engine::InputManager inputManager;
        petriterm::engine::SceneManager sceneManager;

        const auto dimensions = terminal.currentDimensions();
        WorldGrid world = generateWorld(petriterm::world::kDefaultWorldWidthInTiles,
                                        petriterm::world::kDefaultWorldHeightInTiles,
                                        kBootstrapWorldSeed);
        sceneManager.pushScene(std::make_unique<WorldViewScene>(
            std::move(world), dimensions.columns, dimensions.rows));

        petriterm::engine::GameLoop gameLoop(60, 0.0);
        gameLoop.runUntilExitRequested(sceneManager, inputManager, renderer);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "PetriTerm fatal error: %s\n", error.what());
        return 1;
    }
    return 0;
}
