#pragma once

#include <vector>

#include "petriterm/engine/RandomNumberGenerator.hpp"
#include "petriterm/organisms/Species.hpp"
#include "petriterm/world/WorldGrid.hpp"

namespace petriterm::game {

/// How many individuals of each category to seed per hundred tiles, so a starter
/// world is stocked at the same density whatever its size.
///
/// The shape of this is a trophic pyramid: many producers, fewer grazers, fewer
/// predators still. The absolute numbers are a starting point for tuning, not a
/// balanced ecosystem - what they should be is exactly what the headless runs
/// exist to find out.
struct StarterDensityPerHundredTiles {
    double plants = 8.0;
    double herbivores = 1.5;
    double omnivores = 0.4;
    double carnivores = 0.25;
    double decomposers = 1.5;
};

/// Detritus placed on every tile at the start, in energy units.
///
/// Not decoration. Decomposers eat detritus and nothing else, so seeding them
/// into a world where nothing has died yet starves them out within a few ticks,
/// long before the first plant dies of old age to feed them. A starter world is
/// meant to read as an ecosystem already running, and one of those has dead
/// matter in the ground.
inline constexpr double kStarterDetritusPerTile = 6.0;

/// Fitness a tile must offer a species before an individual is seeded onto it.
/// Placing into ground a species cannot survive on just spends the first hundred
/// ticks of every run watching arbitrary seeded organisms starve, which buries
/// whatever the run was meant to measure.
inline constexpr double kMinimumFitnessToSeed = 0.35;

/// Stocks the world with a full food web drawn from the given palette, and seeds
/// starting detritus. Every individual is placed on a tile whose climate actually
/// suits its species, found by sampling tiles at random and rejecting unsuitable
/// ones, so a species only appears in terrain it can hold.
///
/// Draws every random choice from the given generator in a fixed order, so one
/// seed reproduces one starting world exactly - the soak runs depend on that as
/// much as the engine does.
///
/// This is the stand-in for a real scenario definition. When scenarios arrive they
/// own the starting palette and densities, and this becomes one of them.
void seedStarterEcosystem(world::WorldGrid& world,
                          const std::vector<const organisms::Species*>& palette,
                          engine::RandomNumberGenerator& random,
                          const StarterDensityPerHundredTiles& density = {});

}
