#include "petriterm/game/StarterEcosystem.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "petriterm/organisms/Organism.hpp"
#include "petriterm/simulation/SimulationEngine.hpp"
#include "petriterm/world/Tile.hpp"

namespace petriterm::game {

namespace {

using organisms::Organism;
using organisms::OrganismCategory;
using organisms::Species;
using world::Tile;
using world::WorldGrid;

/// Tiles sampled per individual before giving up on placing it. Rejection
/// sampling rather than an exhaustive search of suitable tiles: a species with a
/// narrow band has few homes in a mixed world, and scanning the whole grid for
/// each of several hundred individuals costs far more than occasionally failing to
/// place one.
constexpr int kPlacementAttemptsPerIndividual = 24;

constexpr double kTilesPerDensityUnit = 100.0;

/// Returns the seeded population for one category at the given density, rounded
/// to at least one individual whenever the density is positive, so a small test
/// world still gets a food web rather than silently dropping its predators.
int seededCountFor(double densityPerHundredTiles, int tileCount) {
    if (densityPerHundredTiles <= 0.0 || tileCount <= 0) {
        return 0;
    }
    const double exactCount = densityPerHundredTiles * tileCount / kTilesPerDensityUnit;
    return static_cast<int>(std::max(1.0, std::round(exactCount)));
}

/// Returns the configured density for the given category.
double densityForCategory(const StarterDensityPerHundredTiles& density,
                          OrganismCategory category) {
    switch (category) {
        case OrganismCategory::Plant:
            return density.plants;
        case OrganismCategory::Herbivore:
            return density.herbivores;
        case OrganismCategory::Carnivore:
            return density.carnivores;
        case OrganismCategory::Omnivore:
            return density.omnivores;
        case OrganismCategory::Decomposer:
            return density.decomposers;
    }
    return 0.0;
}

/// Places one individual of the species on a randomly chosen tile that suits it
/// and has room, returning true if it found one within the attempt budget.
bool trySeedOneIndividual(WorldGrid& world, const Species& species,
                          engine::RandomNumberGenerator& random) {
    for (int attempt = 0; attempt < kPlacementAttemptsPerIndividual; ++attempt) {
        const int columnIndex = random.integerInRange(0, world.widthInTiles() - 1);
        const int rowIndex = random.integerInRange(0, world.heightInTiles() - 1);
        Tile& tile = world.tileAt(columnIndex, rowIndex);
        if (!tile.hasCapacityForCategory(species.category)) {
            continue;
        }
        const double fitness = simulation::environmentalFitness(
            species.traits, tile.currentTemperatureCelsius, tile.currentHumidityPercent);
        if (fitness < kMinimumFitnessToSeed) {
            continue;
        }
        tile.occupyingOrganisms.push_back(
            std::make_unique<Organism>(&species, columnIndex, rowIndex));
        return true;
    }
    return false;
}

}

void seedStarterEcosystem(WorldGrid& world, const std::vector<const Species*>& palette,
                          engine::RandomNumberGenerator& random,
                          const StarterDensityPerHundredTiles& density) {
    const int tileCount = world.widthInTiles() * world.heightInTiles();

    world.forEachTile(
        [](int, int, Tile& tile) { tile.detritusLevel = kStarterDetritusPerTile; });

    // Walked in palette order, and every species of a category gets an equal share
    // of that category's budget. Ordering the loop by the palette rather than by
    // category keeps the RNG draw sequence a function of the species file, which is
    // what makes a seeded run reproducible across builds.
    for (const Species* species : palette) {
        if (species == nullptr) {
            continue;
        }
        int speciesInCategory = 0;
        for (const Species* candidate : palette) {
            if (candidate != nullptr && candidate->category == species->category) {
                ++speciesInCategory;
            }
        }
        const int categoryTotal =
            seededCountFor(densityForCategory(density, species->category), tileCount);
        const int shareForThisSpecies =
            speciesInCategory > 0 ? categoryTotal / speciesInCategory : 0;

        for (int index = 0; index < shareForThisSpecies; ++index) {
            trySeedOneIndividual(world, *species, random);
        }
    }
}

}
