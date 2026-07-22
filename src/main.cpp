#include <clocale>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include <ncurses.h>

#include "petriterm/engine/ColorPalette.hpp"
#include "petriterm/engine/GameLoop.hpp"
#include "petriterm/engine/InputManager.hpp"
#include "petriterm/engine/RandomNumberGenerator.hpp"
#include "petriterm/engine/Renderer.hpp"
#include "petriterm/engine/Scene.hpp"
#include "petriterm/engine/SceneManager.hpp"
#include "petriterm/engine/TerminalWindow.hpp"
#include "petriterm/game/Viewport.hpp"
#include "petriterm/world/Biome.hpp"
#include "petriterm/world/ClimateSystem.hpp"
#include "petriterm/world/WorldGenerator.hpp"
#include "petriterm/world/WorldGrid.hpp"

namespace {

using namespace petriterm::engine;
using petriterm::game::ScreenCell;
using petriterm::game::Viewport;
using petriterm::world::BiomeDescriptor;
using petriterm::world::ClimateSystem;
using petriterm::world::describeBiome;
using petriterm::world::describeSeason;
using petriterm::world::describeWeatherPattern;
using petriterm::world::generateWorld;
using petriterm::world::Tile;
using petriterm::world::WorldGrid;

constexpr int kMinimumTerminalColumns = 80;
constexpr int kMinimumTerminalRows = 24;
constexpr std::uint64_t kBootstrapWorldSeed = 42;

/// Bootstrap scene that renders a generated world, advances the climate each
/// tick, and reports live weather in a minimal HUD; the arrow keys scroll the
/// camera. Proves world generation, the climate system, and the viewport
/// integrate. Replaced by the real simulation screen in a later milestone.
class WorldViewScene : public Scene {
public:
    WorldViewScene(WorldGrid generatedWorld, int screenColumns, int screenRows)
        : world(std::move(generatedWorld)),
          climateRandom(kBootstrapWorldSeed),
          climate(climateRandom),
          helpBarRow(screenRows - 1),
          viewport(world.widthInTiles(), world.heightInTiles(), 0, 0, screenColumns,
                   screenRows - 1) {}

    void update(double) override { climate.advanceWeatherAndApplyToWorld(world); }

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
        drawClimateHud(renderer);
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
    /// Draws the minimal weather/season HUD, including the live climate at the
    /// tile in the center of the visible area.
    void drawClimateHud(Renderer& renderer) const {
        const int sampleColumn =
            viewport.cameraColumnIndex() + viewport.visibleWidthInTiles() / 2;
        const int sampleRow =
            viewport.cameraRowIndex() + viewport.visibleHeightInTiles() / 2;
        const Tile& sampleTile = world.tileAt(sampleColumn, sampleRow);

        const std::string weatherLine = std::format(
            "WEATHER: {}", describeWeatherPattern(climate.currentWeatherPattern()));
        const std::string seasonLine =
            std::format("SEASON:  {}", describeSeason(climate.currentSeason()));
        const std::string climateLine = std::format(
            "CENTER TILE: {:.1f}C  {:.0f}% humidity", sampleTile.currentTemperatureCelsius,
            sampleTile.currentHumidityPercent);

        renderer.drawText(0, 0, weatherLine, TerminalColor::Yellow, TerminalColor::Black);
        renderer.drawText(0, 1, seasonLine, TerminalColor::Yellow, TerminalColor::Black);
        renderer.drawText(0, 2, climateLine, TerminalColor::White, TerminalColor::Black);
    }

    WorldGrid world;
    RandomNumberGenerator climateRandom;
    ClimateSystem climate;
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

        petriterm::engine::GameLoop gameLoop(60, 30.0);
        gameLoop.runUntilExitRequested(sceneManager, inputManager, renderer);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "PetriTerm fatal error: %s\n", error.what());
        return 1;
    }
    return 0;
}
