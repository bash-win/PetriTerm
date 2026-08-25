#include "petriterm/game/HeadlessRun.hpp"

#include <utility>

#include "petriterm/engine/RandomNumberGenerator.hpp"
#include "petriterm/simulation/SimulationEngine.hpp"
#include "petriterm/world/WorldGenerator.hpp"
#include "petriterm/world/WorldGrid.hpp"

namespace petriterm::game {

void runHeadlessSimulation(
    const HeadlessRunSettings& settings,
    const std::vector<const organisms::Species*>& palette,
    const std::function<void(const simulation::EcosystemCensus&)>& onCensusSampled) {
    const int widthInTiles = settings.worldWidthInTiles > 0
                                 ? settings.worldWidthInTiles
                                 : world::kDefaultWorldWidthInTiles;
    const int heightInTiles = settings.worldHeightInTiles > 0
                                  ? settings.worldHeightInTiles
                                  : world::kDefaultWorldHeightInTiles;

    world::WorldGrid world =
        world::generateWorld(widthInTiles, heightInTiles, settings.worldSeed);

    // One generator for the starter placement and the run that follows, so the
    // seed covers both. Seeding the ecosystem before the engine takes the world
    // means the first tick already has a food web to advance.
    engine::RandomNumberGenerator random(settings.worldSeed);
    seedStarterEcosystem(world, palette, random, settings.density);

    simulation::SimulationEngine simulation(std::move(world), random);

    for (int tick = 1; tick <= settings.ticksToRun; ++tick) {
        simulation.advanceOneTick();
        const bool isIntervalTick = settings.censusIntervalInTicks > 0 &&
                                    tick % settings.censusIntervalInTicks == 0;
        if (isIntervalTick || tick == settings.ticksToRun) {
            onCensusSampled(simulation::sampleEcosystemCensus(simulation));
        }
    }
}

}
