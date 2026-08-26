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
/// predators still. The ratios between the tiers are the point; the absolute
/// numbers only have to get the web to the first generation, after which breeding
/// decides the standing populations.
///
/// The grazer tier is seeded well above what a standing pyramid would hold, and the
/// predator tiers below it. That looks backwards and is not: what these numbers have
/// to survive is the first fifty ticks, before anything has bred, and the seeded
/// predators can hunt from tick one while the seeded grazers still need a dozen
/// ticks to feed up to their own breeding threshold. Seeded at a realistic standing
/// ratio the predators simply eat the founding herd before it reproduces once, and
/// the top of the web is then decided by that rather than by the ecology. Starting
/// the grazers ahead of where they will settle buys them the head start that a
/// scenario mid-run would already have.
///
/// The producer tier is seeded at something approaching the standing crop the world
/// settles at rather than at a token scattering, for the same reason. Seeded thin,
/// the plants need a hundred ticks to spread into a meadow, and the founding
/// herbivores are dead of starvation before it arrives - not eaten, not badly
/// placed, simply seeded into a world with an eightieth of the forage it will have
/// once it is running. Everything above the producers was failing on that.
///
/// These are per category, and a category's budget is split evenly between the
/// species in it, so adding a species to `species.txt` divides what each of that
/// category's existing species gets rather than adding to the total. The omnivore
/// figure looks high against the carnivore one because it is feeding three species
/// each confined to one narrow climate, not one species with the run of the map.
struct StarterDensityPerHundredTiles {
    double plants = 60.0;
    double herbivores = 4.0;
    double omnivores = 2.0;
    double carnivores = 0.5;
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

/// Energy a seeded individual carries, as a multiple of its species' reproduction
/// threshold. An `Organism` is otherwise born at half that, which is right for
/// something that has just been born and wrong for everything here.
///
/// The same reasoning as the starting detritus above, and the same consequences for
/// getting it wrong. A starter world seeded at newborn energy is not a young
/// ecosystem, it is an ecosystem in which every single organism is simultaneously
/// four ticks from starving: the plants are all seedlings with nothing on them
/// above the root stock a grazer can take, so the herbivores placed beside them
/// starve in a meadow, and the predators placed beside those starve a few ticks
/// later. Every trophic level above the plants was being decided by that rather
/// than by anything the simulation does afterwards.
///
/// Above 1.0 so a seeded organism is not merely alive but grown: fed enough to
/// breed once it settles, and - for a plant - carrying enough growth to be worth
/// grazing on the first tick.
inline constexpr double kStarterEnergyMultipleOfReproductionThreshold = 1.5;

/// Fitness a tile must offer a species before an individual is seeded onto it.
/// Placing into ground a species cannot survive on just spends the first hundred
/// ticks of every run watching arbitrary seeded organisms starve, which buries
/// whatever the run was meant to measure.
///
/// Well above the fitness an organism needs merely to break even, because weather
/// moves every tile's climate at once and a population seeded at its break-even
/// point has nowhere to give. What this buys is headroom: the margin between this
/// and break-even is how much of a cold snap a starter world can absorb before it
/// starts losing individuals rather than just growth.
inline constexpr double kMinimumFitnessToSeed = 0.55;

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
