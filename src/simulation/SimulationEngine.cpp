#include "petriterm/simulation/SimulationEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>

#include "petriterm/organisms/Species.hpp"
#include "petriterm/world/Tile.hpp"

namespace petriterm::simulation {

namespace {

using organisms::Organism;
using organisms::OrganismCategory;
using organisms::TraitProfile;
using world::Tile;
using world::TileCoordinate;
using world::WorldGrid;

/// Energy ceiling, as a multiple of the species' reproduction threshold. Without a
/// cap a well-fed predator banks energy without limit and stops needing to hunt
/// for the rest of its life, which flattens the whole predator-prey cycle.
constexpr double kEnergyCapMultipleOfReproductionThreshold = 2.0;

/// Floor on a tolerance range when computing fitness, so a species file declaring
/// a zero-width tolerance band cannot divide by zero.
constexpr double kMinimumToleranceRange = 1e-6;

/// Fitness an organism needs before it will breed at all. Breeding right at the
/// edge of the tolerance band only produces offspring that starve immediately,
/// which reads on screen as a bug rather than as ecology.
constexpr double kMinimumFitnessToReproduce = 0.15;

/// Chance per tick that an organism with nothing edible in reach moves one tile.
/// Well below 1.0 so a hungry population drifts across the map instead of
/// twitching every tick.
constexpr double kWanderProbabilityPerTick = 0.25;

/// The largest share of a grazed plant's remaining energy one feeding can strip.
///
/// Some limit here is load-bearing. A herbivore feeds on every tick it is hungry,
/// and its breeding cooldown is far shorter than the time a plant needs to grow,
/// so letting a grazer take its whole appetite lets the herd outrun the meadow and
/// strip a seeded world bare inside a couple of hundred ticks - after which the
/// herbivores starve too, and nothing is left alive.
///
/// This particular value is provisional. It buys a world that lives, but it also
/// makes a mouthful off a seedling so small that herbivores cannot hold a
/// population for long. Landing that trade-off properly needs the sweep across
/// seeds and species data that the tuning milestone exists to do.
constexpr double kMaximumGrazedFractionPerFeeding = 0.5;

/// Visits every in-bounds tile at exactly the given Chebyshev distance from the
/// center, letting callers search outward one ring at a time. A radius of zero
/// visits the center tile itself.
void forEachTileAtChebyshevRadius(const WorldGrid& world, int centerColumnIndex,
                                  int centerRowIndex, int radius,
                                  const std::function<void(int, int)>& visit) {
    const auto visitIfInBounds = [&](int columnIndex, int rowIndex) {
        if (world.containsCoordinate(columnIndex, rowIndex)) {
            visit(columnIndex, rowIndex);
        }
    };

    if (radius <= 0) {
        visitIfInBounds(centerColumnIndex, centerRowIndex);
        return;
    }

    // The top and bottom edges of the ring, corners included, then the left and
    // right edges with the corners left out so no tile is visited twice.
    for (int columnOffset = -radius; columnOffset <= radius; ++columnOffset) {
        visitIfInBounds(centerColumnIndex + columnOffset, centerRowIndex - radius);
        visitIfInBounds(centerColumnIndex + columnOffset, centerRowIndex + radius);
    }
    for (int rowOffset = -radius + 1; rowOffset <= radius - 1; ++rowOffset) {
        visitIfInBounds(centerColumnIndex - radius, centerRowIndex + rowOffset);
        visitIfInBounds(centerColumnIndex + radius, centerRowIndex + rowOffset);
    }
}

/// Returns the organism's energy ceiling, or zero for a degenerate species that
/// declares no reproduction threshold to scale it from.
double energyCapFor(const Organism& organism) {
    return organism.species->traits.energyRequiredToReproduce *
           kEnergyCapMultipleOfReproductionThreshold;
}

/// Returns true if the organism has room for more energy, which is what makes it
/// look for food. An organism at its ceiling ignores food entirely.
bool isHungry(const Organism& organism) {
    const double energyCap = energyCapFor(organism);
    return energyCap <= 0.0 || organism.remainingEnergyUnits < energyCap;
}

/// Adds energy, stopping at the organism's ceiling.
void addEnergyUpToCap(Organism& organism, double energyGained) {
    const double energyCap = energyCapFor(organism);
    if (energyCap <= 0.0) {
        organism.remainingEnergyUnits += energyGained;
        return;
    }
    organism.remainingEnergyUnits =
        std::min(organism.remainingEnergyUnits + energyGained, energyCap);
}

}

double environmentalFitness(const TraitProfile& traits, double temperatureCelsius,
                            double humidityPercent) {
    const double temperatureRange =
        std::max(traits.temperatureToleranceRange, kMinimumToleranceRange);
    const double humidityRange =
        std::max(traits.humidityToleranceRange, kMinimumToleranceRange);
    const double temperatureFit =
        1.0 -
        std::abs(temperatureCelsius - traits.idealTemperatureCelsius) / temperatureRange;
    const double humidityFit =
        1.0 - std::abs(humidityPercent - traits.idealHumidityPercent) / humidityRange;
    return std::clamp(temperatureFit, 0.0, 1.0) * std::clamp(humidityFit, 0.0, 1.0);
}

int TickReport::livingCountOf(OrganismCategory category) const {
    return livingCountByCategory[static_cast<std::size_t>(category)];
}

SimulationEngine::SimulationEngine(WorldGrid initialWorld,
                                   engine::RandomNumberGenerator& sharedRandom)
    : worldGrid(std::move(initialWorld)),
      sharedRandom(sharedRandom),
      climateSystem(sharedRandom) {}

WorldGrid& SimulationEngine::world() {
    return worldGrid;
}

const WorldGrid& SimulationEngine::world() const {
    return worldGrid;
}

const world::ClimateSystem& SimulationEngine::climate() const {
    return climateSystem;
}

std::uint64_t SimulationEngine::tickIndex() const {
    return simulatedTickCount;
}

const TickReport& SimulationEngine::latestTickReport() const {
    return lastTickReport;
}

const TickReport& SimulationEngine::advanceOneTick() {
    lastTickReport = TickReport{};
    climateSystem.advanceWeatherAndApplyToWorld(worldGrid);
    applyMetabolismToEveryOrganism();
    runBehaviorForEveryOrganism();
    removeDeadAndTakeCensus();
    ++simulatedTickCount;
    return lastTickReport;
}

void SimulationEngine::collectLivingOrganisms(std::vector<Organism*>& destination) {
    destination.clear();
    worldGrid.forEachTile([&destination](int, int, Tile& tile) {
        for (const auto& occupant : tile.occupyingOrganisms) {
            if (occupant->isAlive) {
                destination.push_back(occupant.get());
            }
        }
    });
}

double SimulationEngine::fitnessOf(const Organism& organism) const {
    const Tile& tile = worldGrid.tileAt(organism.tileColumnIndex, organism.tileRowIndex);
    return environmentalFitness(organism.species->traits, tile.currentTemperatureCelsius,
                                tile.currentHumidityPercent);
}

void SimulationEngine::applyMetabolismToEveryOrganism() {
    collectLivingOrganisms(organismSnapshot);
    for (Organism* organism : organismSnapshot) {
        organism->applyMetabolismAndAgingForOneTick();
    }
}

void SimulationEngine::runBehaviorForEveryOrganism() {
    collectLivingOrganisms(organismSnapshot);
    for (Organism* organism : organismSnapshot) {
        // Something earlier in this phase may have eaten it.
        if (!organism->isAlive) {
            continue;
        }
        const double fitness = fitnessOf(*organism);
        switch (organism->species->category) {
            case OrganismCategory::Plant:
                actAsPlant(*organism, fitness);
                break;
            case OrganismCategory::Decomposer:
                actAsDecomposer(*organism, fitness);
                break;
            case OrganismCategory::Herbivore:
            case OrganismCategory::Carnivore:
            case OrganismCategory::Omnivore:
                actAsConsumer(*organism, fitness);
                break;
        }
    }
}

void SimulationEngine::actAsPlant(Organism& organism, double fitness) {
    // Photosynthesis: no prey to find and nowhere to go. Yield scales with
    // fitness, so a plant outside its band cannot cover its own upkeep and
    // withers where it stands.
    if (isHungry(organism)) {
        const double energyGained =
            organism.species->traits.energyGainedPerFeeding * fitness;
        if (energyGained > 0.0) {
            addEnergyUpToCap(organism, energyGained);
            ++lastTickReport.feedingCount;
        }
    }
    // Seeds only take on some ticks, which is what keeps a meadow spreading at a
    // pace the player can watch rather than filling the map the moment it can
    // afford to.
    if (sharedRandom.chance(organism.species->traits.seedDispersalProbabilityPerTick)) {
        tryReproduce(organism, fitness);
    }
}

void SimulationEngine::actAsDecomposer(Organism& organism, double fitness) {
    if (!isHungry(organism) || !scavengeWithinReach(organism, fitness)) {
        wanderOneTile(organism);
    }
    tryReproduce(organism, fitness);
}

void SimulationEngine::actAsConsumer(Organism& organism, double fitness) {
    if (isHungry(organism) && !grazeOrHuntWithinReach(organism, fitness)) {
        wanderOneTile(organism);
    }
    tryReproduce(organism, fitness);
}

Organism* SimulationEngine::findNearestTargetWithinReach(
    const Organism& seeker, const std::function<bool(const Organism&)>& isTarget) {
    const int reachInTiles = std::max(0, seeker.species->traits.movementRangeInTiles);
    for (int radius = 0; radius <= reachInTiles; ++radius) {
        targetCandidates.clear();
        forEachTileAtChebyshevRadius(
            worldGrid, seeker.tileColumnIndex, seeker.tileRowIndex, radius,
            [this, &seeker, &isTarget](int columnIndex, int rowIndex) {
                for (const auto& occupant :
                     worldGrid.tileAt(columnIndex, rowIndex).occupyingOrganisms) {
                    if (occupant.get() != &seeker && isTarget(*occupant)) {
                        targetCandidates.push_back(occupant.get());
                    }
                }
            });
        if (!targetCandidates.empty()) {
            return sharedRandom.pickUniformly(targetCandidates);
        }
    }
    return nullptr;
}

bool SimulationEngine::grazeOrHuntWithinReach(Organism& organism, double fitness) {
    const organisms::Species& species = *organism.species;
    Organism* prey =
        findNearestTargetWithinReach(organism, [&species](const Organism& candidate) {
            return candidate.isAlive &&
                   species.canConsumeCategory(candidate.species->category);
        });
    if (prey == nullptr) {
        return false;
    }

    const double appetite = species.traits.energyGainedPerFeeding * fitness;
    if (appetite <= 0.0) {
        return false;
    }

    double energyTaken = 0.0;
    if (prey->species->category == OrganismCategory::Plant) {
        // Grazing crops the plant and leaves it standing unless that empties it,
        // which is what lets a meadow carry grazers at all instead of being
        // consumed once and never recovering.
        energyTaken = std::min(
            appetite, prey->remainingEnergyUnits * kMaximumGrazedFractionPerFeeding);
        prey->remainingEnergyUnits -= energyTaken;
        if (prey->remainingEnergyUnits <= 0.0) {
            prey->isAlive = false;
        }
    } else {
        // Animal prey is killed outright. A predator that only wounded its target
        // could never cover its upkeep at these energy costs.
        energyTaken = appetite;
        prey->isAlive = false;
    }
    if (energyTaken <= 0.0) {
        return false;
    }

    addEnergyUpToCap(organism, energyTaken);
    ++lastTickReport.feedingCount;
    // Close onto the prey's tile so the chase is legible on the map. A kill frees
    // the capacity that makes room for the hunter.
    moveIfTileHasRoom(organism, prey->tileColumnIndex, prey->tileRowIndex);
    return true;
}

bool SimulationEngine::scavengeWithinReach(Organism& organism, double fitness) {
    Organism* corpse = findNearestTargetWithinReach(
        organism, [](const Organism& candidate) { return !candidate.isAlive; });
    if (corpse == nullptr) {
        return false;
    }
    const double energyGained = organism.species->traits.energyGainedPerFeeding * fitness;
    if (energyGained <= 0.0) {
        return false;
    }
    // A corpse is not consumed, so several decomposers can work the same one in a
    // tick. It is cleared in phase 4 regardless.
    addEnergyUpToCap(organism, energyGained);
    ++lastTickReport.feedingCount;
    moveIfTileHasRoom(organism, corpse->tileColumnIndex, corpse->tileRowIndex);
    return true;
}

void SimulationEngine::wanderOneTile(Organism& organism) {
    if (organism.species->traits.movementRangeInTiles <= 0) {
        return;
    }
    if (!sharedRandom.chance(kWanderProbabilityPerTick)) {
        return;
    }
    const OrganismCategory category = organism.species->category;
    tileCandidates.clear();
    forEachTileAtChebyshevRadius(
        worldGrid, organism.tileColumnIndex, organism.tileRowIndex, 1,
        [this, category](int columnIndex, int rowIndex) {
            if (worldGrid.tileAt(columnIndex, rowIndex).hasCapacityForCategory(category)) {
                tileCandidates.push_back(TileCoordinate{columnIndex, rowIndex});
            }
        });
    if (tileCandidates.empty()) {
        return;
    }
    const TileCoordinate destination = sharedRandom.pickUniformly(tileCandidates);
    moveIfTileHasRoom(organism, destination.columnIndex, destination.rowIndex);
}

bool SimulationEngine::tryReproduce(Organism& parent, double fitness) {
    if (!parent.isReadyToReproduce() || fitness < kMinimumFitnessToReproduce) {
        return false;
    }
    const OrganismCategory category = parent.species->category;
    tileCandidates.clear();
    // The parent's own tile first, then the ring around it, so a colony spreads
    // outward from where it started instead of scattering.
    for (int radius = 0; radius <= 1; ++radius) {
        forEachTileAtChebyshevRadius(
            worldGrid, parent.tileColumnIndex, parent.tileRowIndex, radius,
            [this, category](int columnIndex, int rowIndex) {
                if (worldGrid.tileAt(columnIndex, rowIndex)
                        .hasCapacityForCategory(category)) {
                    tileCandidates.push_back(TileCoordinate{columnIndex, rowIndex});
                }
            });
    }
    if (tileCandidates.empty()) {
        return false;
    }

    const TileCoordinate nursery = sharedRandom.pickUniformly(tileCandidates);
    parent.payReproductionCostAndResetCooldown();
    worldGrid.tileAt(nursery.columnIndex, nursery.rowIndex)
        .occupyingOrganisms.push_back(std::make_unique<Organism>(
            parent.species, nursery.columnIndex, nursery.rowIndex));
    ++lastTickReport.birthCount;
    return true;
}

void SimulationEngine::moveIfTileHasRoom(Organism& organism, int destinationColumnIndex,
                                         int destinationRowIndex) {
    if (organism.tileColumnIndex == destinationColumnIndex &&
        organism.tileRowIndex == destinationRowIndex) {
        return;
    }
    if (!worldGrid.containsCoordinate(destinationColumnIndex, destinationRowIndex)) {
        return;
    }
    Tile& destinationTile = worldGrid.tileAt(destinationColumnIndex, destinationRowIndex);
    if (!destinationTile.hasCapacityForCategory(organism.species->category)) {
        return;
    }

    Tile& sourceTile = worldGrid.tileAt(organism.tileColumnIndex, organism.tileRowIndex);
    const auto owned = std::find_if(sourceTile.occupyingOrganisms.begin(),
                                    sourceTile.occupyingOrganisms.end(),
                                    [&organism](const std::unique_ptr<Organism>& occupant) {
                                        return occupant.get() == &organism;
                                    });
    if (owned == sourceTile.occupyingOrganisms.end()) {
        return;
    }

    std::unique_ptr<Organism> transferred = std::move(*owned);
    sourceTile.occupyingOrganisms.erase(owned);
    organism.tileColumnIndex = destinationColumnIndex;
    organism.tileRowIndex = destinationRowIndex;
    destinationTile.occupyingOrganisms.push_back(std::move(transferred));
}

void SimulationEngine::removeDeadAndTakeCensus() {
    worldGrid.forEachTile([this](int, int, Tile& tile) {
        auto& occupants = tile.occupyingOrganisms;
        const auto firstDead = std::remove_if(
            occupants.begin(), occupants.end(),
            [](const std::unique_ptr<Organism>& occupant) { return !occupant->isAlive; });
        lastTickReport.deathCount +=
            static_cast<int>(std::distance(firstDead, occupants.end()));
        occupants.erase(firstDead, occupants.end());

        for (const auto& occupant : occupants) {
            ++lastTickReport.livingCountByCategory[static_cast<std::size_t>(
                occupant->species->category)];
            ++lastTickReport.totalLivingCount;
        }
    });
}

}
