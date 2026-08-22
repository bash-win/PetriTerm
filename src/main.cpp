#include <clocale>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "petriterm/engine/ColorPalette.hpp"
#include "petriterm/engine/GameLoop.hpp"
#include "petriterm/engine/InputManager.hpp"
#include "petriterm/engine/RandomNumberGenerator.hpp"
#include "petriterm/engine/Renderer.hpp"
#include "petriterm/engine/Scene.hpp"
#include "petriterm/engine/SceneManager.hpp"
#include "petriterm/engine/SimulationClock.hpp"
#include "petriterm/engine/TerminalWindow.hpp"
#include "petriterm/game/PlacementController.hpp"
#include "petriterm/game/Viewport.hpp"
#include "petriterm/organisms/Organism.hpp"
#include "petriterm/organisms/Species.hpp"
#include "petriterm/organisms/SpeciesRegistry.hpp"
#include "petriterm/simulation/SimulationEngine.hpp"
#include "petriterm/world/Biome.hpp"
#include "petriterm/world/ClimateSystem.hpp"
#include "petriterm/world/WorldGenerator.hpp"
#include "petriterm/world/WorldGrid.hpp"

namespace {

using namespace petriterm::engine;
using petriterm::engine::SimulationClock;
using petriterm::game::PlacementController;
using petriterm::game::ScreenCell;
using petriterm::game::Viewport;
using petriterm::organisms::Organism;
using petriterm::organisms::OrganismCategory;
using petriterm::organisms::Species;
using petriterm::organisms::SpeciesRegistry;
using petriterm::simulation::SimulationEngine;
using petriterm::simulation::TickReport;
using petriterm::world::BiomeDescriptor;
using petriterm::world::describeBiome;
using petriterm::world::describeSeason;
using petriterm::world::describeWeatherPattern;
using petriterm::world::generateWorld;
using petriterm::world::Tile;
using petriterm::world::WorldGrid;

constexpr std::uint64_t kBootstrapWorldSeed = 42;
constexpr int kStartingEcoCredits = 50;

/// Returns a drawing priority so the highest trophic level present on a tile is
/// the one shown: carnivore > omnivore > herbivore > plant > decomposer.
int trophicDrawPriority(OrganismCategory category) {
    switch (category) {
        case OrganismCategory::Carnivore:
            return 4;
        case OrganismCategory::Omnivore:
            return 3;
        case OrganismCategory::Herbivore:
            return 2;
        case OrganismCategory::Plant:
            return 1;
        case OrganismCategory::Decomposer:
            return 0;
    }
    return 0;
}

/// Returns the living organism on the tile with the highest trophic draw
/// priority, or nullptr if the tile has no living organisms.
const Organism* dominantLivingOrganism(const Tile& tile) {
    const Organism* dominant = nullptr;
    int highestPriority = -1;
    for (const auto& organism : tile.occupyingOrganisms) {
        if (!organism->isAlive) {
            continue;
        }
        const int priority = trophicDrawPriority(organism->species->category);
        if (priority > highestPriority) {
            highestPriority = priority;
            dominant = organism.get();
        }
    }
    return dominant;
}

/// Locates the species data file next to the executable, falling back to the
/// current directory.
std::filesystem::path locateSpeciesFile() {
    std::error_code errorCode;
    const std::filesystem::path executablePath =
        std::filesystem::read_symlink("/proc/self/exe", errorCode);
    if (!errorCode) {
        const std::filesystem::path candidate =
            executablePath.parent_path() / "species.txt";
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return "species.txt";
}

/// Bootstrap scene for placing organisms: a cursor moves over the world (the
/// camera follows), Tab cycles the species palette, and Enter places the selected
/// species for its eco-credit cost. Space pauses, +/- change speed, and '.'
/// single-steps. The simulation runs underneath, so placed organisms feed, breed,
/// and die while the HUD reports the census. Replaced by the real simulation
/// screen in a later milestone.
class WorldViewScene : public Scene {
public:
    WorldViewScene(SimulationEngine& simulation, const SpeciesRegistry& registry,
                   SimulationClock& simulationClock, int screenColumns, int screenRows)
        : simulation(simulation),
          placement(simulation.world().widthInTiles(), simulation.world().heightInTiles(),
                    registry.allSpecies()),
          simulationClock(simulationClock),
          ecoCreditBalance(kStartingEcoCredits),
          helpBarRow(screenRows - 1),
          viewport(simulation.world().widthInTiles(), simulation.world().heightInTiles(), 0,
                   0, screenColumns, screenRows - 1) {}

    void update(double) override { simulation.advanceOneTick(); }

    void render(Renderer& renderer) override {
        constexpr std::string_view hint =
            "arrows: cursor  tab: species  enter: place  space: pause  +/-: speed  "
            ".: step  q: quit";
        renderer.beginFrame();
        const WorldGrid& world = simulation.world();
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
                const bool isCursorTile = tileColumn == placement.cursorColumnIndex() &&
                                          tileRow == placement.cursorRowIndex();
                drawTile(renderer, *cell, world.tileAt(tileColumn, tileRow), isCursorTile);
            }
        }
        drawHud(renderer);
        renderer.drawText(0, helpBarRow, hint, TextStyle{TerminalColor::Cyan});
        renderer.endFrame();
    }

    SceneTransition handleKeyEvent(const KeyEvent& event) override {
        switch (event.code) {
            case KeyCode::ArrowUp:
                moveCursor(0, -1);
                break;
            case KeyCode::ArrowDown:
                moveCursor(0, 1);
                break;
            case KeyCode::ArrowLeft:
                moveCursor(-1, 0);
                break;
            case KeyCode::ArrowRight:
                moveCursor(1, 0);
                break;
            case KeyCode::Enter:
                placement.placeSelectedSpeciesAtCursor(simulation.world(),
                                                       ecoCreditBalance);
                break;
            case KeyCode::Space:
                simulationClock.togglePause();
                break;
            case KeyCode::Escape:
                return SceneTransition::exitApplication();
            case KeyCode::Tab:
                placement.selectAdjacentSpeciesInPalette(1);
                break;
            case KeyCode::BackTab:
                placement.selectAdjacentSpeciesInPalette(-1);
                break;
            case KeyCode::Character:
                handleCharacterKey(event.character);
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
    /// Handles the character keys that control playback. Kept separate from the
    /// key-code switch because these are all typed characters rather than named
    /// keys, and both spellings of each are accepted so the player does not have
    /// to reach for shift.
    void handleCharacterKey(wchar_t character) {
        switch (character) {
            case L'+':
            case L'=':
                simulationClock.selectFasterSpeed();
                break;
            case L'-':
            case L'_':
                simulationClock.selectSlowerSpeed();
                break;
            case L'.':
                simulationClock.requestSingleTickStep();
                break;
            default:
                break;
        }
    }

    /// Moves the placement cursor and scrolls the camera the minimum needed to
    /// keep the cursor on screen.
    void moveCursor(int columnDelta, int rowDelta) {
        placement.moveCursorByTiles(columnDelta, rowDelta);
        viewport.ensureTileVisible(placement.cursorColumnIndex(),
                                   placement.cursorRowIndex());
    }

    /// Draws one tile: the dominant organism as a bold glyph in its species color
    /// on a neutral cell, or the biome glyph on the biome's background color. The
    /// cursor tile is inverted on top of whichever of those it is.
    static void drawTile(Renderer& renderer, const ScreenCell& cell, const Tile& tile,
                         bool isCursorTile) {
        if (const Organism* dominant = dominantLivingOrganism(tile)) {
            renderer.drawGlyph(
                cell.columnIndex, cell.rowIndex, dominant->species->glyph,
                TextStyle{dominant->species->glyphColor, TerminalColor::Black,
                          isCursorTile ? TextEmphasis::BoldInverted : TextEmphasis::Bold});
            return;
        }
        const BiomeDescriptor& descriptor = describeBiome(tile.biome);
        renderer.drawGlyph(
            cell.columnIndex, cell.rowIndex, descriptor.backgroundGlyph,
            TextStyle{TerminalColor::Black, descriptor.backgroundColor,
                      isCursorTile ? TextEmphasis::Inverted : TextEmphasis::Normal});
    }

    /// Draws the HUD: live weather/season, the climate at the cursor tile, the
    /// eco-credit balance, the selected palette species, and the census from the
    /// most recent tick.
    void drawHud(Renderer& renderer) const {
        const Tile& cursorTile = simulation.world().tileAt(placement.cursorColumnIndex(),
                                                           placement.cursorRowIndex());

        constexpr TextStyle labelStyle{TerminalColor::Yellow, TerminalColor::Black};
        constexpr TextStyle valueStyle{TerminalColor::White, TerminalColor::Black};

        renderer.drawText(
            0, 0,
            std::format("WEATHER: {}", describeWeatherPattern(
                                           simulation.climate().currentWeatherPattern())),
            labelStyle);
        renderer.drawText(0, 1,
                          std::format("SEASON:  {}",
                                      describeSeason(simulation.climate().currentSeason())),
                          labelStyle);
        renderer.drawText(0, 2,
                          std::format("CURSOR TILE: {:.1f}C  {:.0f}% humidity",
                                      cursorTile.currentTemperatureCelsius,
                                      cursorTile.currentHumidityPercent),
                          valueStyle);
        renderer.drawText(0, 3, std::format("CREDITS: {}", ecoCreditBalance),
                          TextStyle{TerminalColor::Green, TerminalColor::Black});
        renderer.drawText(
            0, 5,
            std::format("SPEED: {}  tick {}", simulationClock.describeSpeed(),
                        simulation.tickIndex()),
            TextStyle{
                simulationClock.isPaused() ? TerminalColor::Yellow : TerminalColor::White,
                TerminalColor::Black,
                simulationClock.isPaused() ? TextEmphasis::Bold : TextEmphasis::Normal});

        const TickReport& report = simulation.latestTickReport();
        renderer.drawText(
            0, 6,
            std::format("LIFE: {}  P{} H{} C{} O{} D{}  +{} -{}", report.totalLivingCount,
                        report.livingCountOf(OrganismCategory::Plant),
                        report.livingCountOf(OrganismCategory::Herbivore),
                        report.livingCountOf(OrganismCategory::Carnivore),
                        report.livingCountOf(OrganismCategory::Omnivore),
                        report.livingCountOf(OrganismCategory::Decomposer),
                        report.birthCount, report.deathCount),
            valueStyle);
        if (simulationClock.isFallingBehind()) {
            renderer.drawText(
                0, 7, "SLOW: simulation is behind the selected speed",
                TextStyle{TerminalColor::Red, TerminalColor::Black, TextEmphasis::Bold});
        }

        const Species* selected = placement.selectedSpecies();
        if (selected != nullptr) {
            renderer.drawText(0, 4, "SELECTED:", valueStyle);
            renderer.drawGlyph(
                10, 4, selected->glyph,
                TextStyle{selected->glyphColor, TerminalColor::Black, TextEmphasis::Bold});
            renderer.drawText(12, 4,
                              std::format("{} ({}c)", selected->displayName,
                                          selected->ecoCreditCostToPlace),
                              valueStyle);
        }
    }

    SimulationEngine& simulation;
    PlacementController placement;
    SimulationClock& simulationClock;
    int ecoCreditBalance;
    int helpBarRow;
    Viewport viewport;
};

}

int main() {
    std::setlocale(LC_ALL, "");
    try {
        SpeciesRegistry speciesRegistry;
        speciesRegistry.loadFromFile(locateSpeciesFile());

        petriterm::engine::TerminalWindow terminal;
        petriterm::engine::ColorPalette palette;
        palette.initializeColorPairs();
        petriterm::engine::Renderer renderer(terminal.rootWindow(), palette);
        petriterm::engine::InputManager inputManager;

        // The RNG and the engine outlive the scene stack that borrows them, so they
        // are declared before the scene manager and destroyed after it.
        RandomNumberGenerator simulationRandom(kBootstrapWorldSeed);
        SimulationEngine simulation(
            generateWorld(petriterm::world::kDefaultWorldWidthInTiles,
                          petriterm::world::kDefaultWorldHeightInTiles,
                          kBootstrapWorldSeed),
            simulationRandom);
        petriterm::engine::SimulationClock simulationClock;

        petriterm::engine::SceneManager sceneManager;
        const auto dimensions = terminal.currentDimensions();
        sceneManager.pushScene(
            std::make_unique<WorldViewScene>(simulation, speciesRegistry, simulationClock,
                                             dimensions.columns, dimensions.rows));

        // Thirty frames a second: nothing on screen changes faster than a tick,
        // and the simulation rate is the clock's business, not the loop's.
        petriterm::engine::GameLoop gameLoop(30, simulationClock);
        gameLoop.runUntilExitRequested(sceneManager, inputManager, renderer, terminal);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "PetriTerm fatal error: %s\n", error.what());
        return 1;
    }
    return 0;
}
