#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "petriterm/engine/RandomNumberGenerator.hpp"
#include "petriterm/organisms/Organism.hpp"
#include "petriterm/organisms/OrganismCategory.hpp"
#include "petriterm/organisms/TraitProfile.hpp"
#include "petriterm/world/ClimateSystem.hpp"
#include "petriterm/world/WorldGrid.hpp"

namespace petriterm::simulation {

/// How well a species' traits suit the climate it is standing in, on [0, 1]: 1.0
/// at its ideal temperature and humidity, falling linearly to 0.0 at the edge of
/// its tolerance band and staying there beyond it. The two axes are multiplied, so
/// being outside either one alone is enough to yield nothing.
///
/// Fitness scales feeding yield and gates breeding rather than killing directly,
/// so a badly placed organism starves over several ticks instead of vanishing.
/// That delay is what leaves the player something to notice and act on.
double environmentalFitness(const organisms::TraitProfile& traits,
                            double temperatureCelsius, double humidityPercent);

/// What one tick did. Populations are counted after the tick's deaths and births
/// have been applied, so they match what the next frame draws.
struct TickReport {
    std::array<int, organisms::kOrganismCategoryCount> livingCountByCategory{};
    int totalLivingCount = 0;
    int birthCount = 0;
    int deathCount = 0;
    int feedingCount = 0;

    /// Returns the living population of the given category.
    int livingCountOf(organisms::OrganismCategory category) const;
};

/// Owns the world and advances it one fixed tick at a time. This is the
/// authoritative simulation state: the tick index here counts simulated history,
/// unlike SimulationClock's counter, which counts playback and is affected by
/// pausing and speed.
///
/// One tick runs four phases, and the order is the contract:
///   1. Climate - every tile's current temperature and humidity are re-derived,
///      so all behavior in the tick reads one consistent set of conditions.
///   2. Metabolism - every living organism burns its upkeep, ages, and counts
///      down its breeding cooldown. Starvation and old-age deaths land here.
///   3. Behavior - the survivors feed, move, and breed.
///   4. Cleanup - the dead are removed and the census is taken.
///
/// Phases 2 and 3 each walk a snapshot of organism pointers taken at the start of
/// the phase. Tiles own their organisms through unique_ptr, so moving one between
/// tiles leaves the pointee's address alone and the snapshot stays valid. Taking
/// the snapshot up front is what stops an organism that moves mid-phase from
/// acting twice, and stops one born this tick from acting on the tick it appeared.
///
/// Every random choice draws from the shared RNG in this fixed order, so one seed
/// reproduces a run exactly.
class SimulationEngine {
public:
    /// Takes ownership of the world to simulate and borrows the RNG seeding the
    /// run. The RNG is shared with the climate system so a single seed drives both
    /// weather and behavior.
    SimulationEngine(world::WorldGrid initialWorld,
                     engine::RandomNumberGenerator& sharedRandom);

    /// Advances the simulation by exactly one tick and returns what happened.
    const TickReport& advanceOneTick();

    world::WorldGrid& world();
    const world::WorldGrid& world() const;

    /// Exposes the climate for display. The engine advances it as part of each
    /// tick, so callers must not advance it themselves.
    const world::ClimateSystem& climate() const;

    /// Ticks simulated since the world was created.
    std::uint64_t tickIndex() const;

    /// The report from the most recent tick, zeroed before the first one runs.
    const TickReport& latestTickReport() const;

private:
    /// One member per tick phase, in the order advanceOneTick runs them.
    void applyMetabolismToEveryOrganism();
    void runBehaviorForEveryOrganism();
    void removeDeadAndTakeCensus();

    /// The behavior branch for each trophic role, selected by the organism's
    /// category. Each receives the fitness already computed for its tile.
    void actAsPlant(organisms::Organism& organism, double fitness);
    void actAsDecomposer(organisms::Organism& organism, double fitness);
    void actAsConsumer(organisms::Organism& organism, double fitness);

    /// Eats the nearest reachable prey the organism's diet allows. Plants are
    /// grazed for part of their energy and survive unless drained; animal prey is
    /// killed outright. Returns true if the organism fed.
    bool grazeOrHuntWithinReach(organisms::Organism& organism, double fitness);

    /// Feeds on the nearest reachable corpse. Corpses are cleared in phase 4, so
    /// what a decomposer finds are the organisms that died earlier this tick.
    bool scavengeWithinReach(organisms::Organism& organism, double fitness);

    /// Moves one tile to a random neighbor with room, and only sometimes, so a
    /// hungry population drifts instead of twitching every tick. Does nothing for
    /// a sessile species.
    void wanderOneTile(organisms::Organism& organism);

    /// Places one offspring on the parent's own tile or a neighbor with room,
    /// charging the parent its species' reproduction cost. Returns true on a birth.
    bool tryReproduce(organisms::Organism& parent, double fitness);

    /// Returns a uniformly chosen organism satisfying the predicate at the
    /// smallest Chebyshev distance within the seeker's reach, or nullptr if none
    /// is in range. Searching outward one ring at a time means an organism always
    /// takes the nearest food rather than any food in range.
    organisms::Organism* findNearestTargetWithinReach(
        const organisms::Organism& seeker,
        const std::function<bool(const organisms::Organism&)>& isTarget);

    /// Transfers ownership of the organism to the given tile and updates its
    /// stored coordinates, if that tile exists and has room for its category.
    void moveIfTileHasRoom(organisms::Organism& organism, int destinationColumnIndex,
                           int destinationRowIndex);

    /// Fills the buffer with pointers to every living organism in row-major tile
    /// order, so a phase can iterate without being disturbed by the moves and
    /// births it causes.
    void collectLivingOrganisms(std::vector<organisms::Organism*>& destination);

    /// The organism's fitness for the tile it currently stands on.
    double fitnessOf(const organisms::Organism& organism) const;

    world::WorldGrid worldGrid;
    engine::RandomNumberGenerator& sharedRandom;
    world::ClimateSystem climateSystem;
    std::uint64_t simulatedTickCount = 0;
    TickReport lastTickReport;

    /// Buffers reused across ticks so a tick allocates nothing per organism. Their
    /// contents mean nothing between uses.
    std::vector<organisms::Organism*> organismSnapshot;
    std::vector<organisms::Organism*> targetCandidates;
    std::vector<world::TileCoordinate> tileCandidates;
};

}
