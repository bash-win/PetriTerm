#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "petriterm/game/HeadlessRun.hpp"
#include "petriterm/organisms/OrganismCategory.hpp"
#include "petriterm/organisms/Species.hpp"
#include "petriterm/organisms/SpeciesRegistry.hpp"
#include "petriterm/simulation/EcosystemCensus.hpp"

using petriterm::game::HeadlessRunSettings;
using petriterm::game::runHeadlessSimulation;
using petriterm::organisms::describeOrganismCategory;
using petriterm::organisms::kOrganismCategoryCount;
using petriterm::organisms::OrganismCategory;
using petriterm::organisms::Species;
using petriterm::organisms::SpeciesRegistry;
using petriterm::simulation::EcosystemCensus;
using petriterm::simulation::formatCensusAsCsvRow;

namespace {

/// What a run has to survive to count as balanced. Not the tuning target - the
/// tuning target is a food web that oscillates - but the floor below which the
/// balance has regressed to the failure this milestone existed to fix.
constexpr int kSoakSeeds[] = {1, 2, 3, 4, 5, 6, 7, 42, 99, 123};
constexpr int kSoakTicks = 5000;

/// Passed as the world size to mean "whatever the game itself uses", which
/// HeadlessRunSettings spells as a non-positive dimension.
constexpr int kDefaultWorldSize = 0;

/// A shorter run, for the tier of this that runs on every build. Shorter but not
/// smaller: the world stays the size the balance was tuned against, because a
/// smaller one is a different ecology rather than a cheaper sample of the same one.
/// Densities are per hundred tiles, so a small world seeds the same proportions,
/// but it holds a fraction of the individuals and none of the spatial refuge that
/// lets a hunted population recover in one corner while it is being cleared out of
/// another - a 64x32 world loses its herbivores on settings the full one carries
/// for five thousand ticks. Trading ticks for time keeps the claim honest; trading
/// tiles for it would not.
///
/// Six hundred ticks because every failure the balance pass was opened for
/// happened inside that: the carnivores were gone by tick 20, the herbivores and
/// omnivores by 140, and the first weather-driven mass death landed at 300. A run
/// this long will not catch slow drift, which is what the full soak below is for,
/// but it catches every way the web is currently known to fall over.
constexpr int kQuickTicks = 600;
constexpr int kQuickSeeds[] = {1, 42};

/// Ticks for the determinism check, which is the one case here that cannot share a
/// run with another and so pays for its own twice over. Short because drift shows
/// up in the first divergent draw or not at all - a run that agrees for two hundred
/// ticks did not get there by luck.
constexpr int kDeterminismTicks = 200;

/// The lowest each category fell to over a whole run, alongside where it ended.
/// The minimum is the number that matters: a food web that loses its predators at
/// tick 300 and is sampled at tick 5000 looks identical to one that never had any.
struct RunOutcome {
    std::array<int, kOrganismCategoryCount> lowestCountByCategory{};
    EcosystemCensus finalCensus;
    int censusCount = 0;
};

/// The shipped species file, loaded once. The soak is a claim about the data in
/// data/species.txt as much as about the engine, so it has to be that file rather
/// than species invented by the test.
const SpeciesRegistry& shippedSpeciesRegistry() {
    static const SpeciesRegistry registry = [] {
        SpeciesRegistry loaded;
        loaded.loadFromFile(PETRITERM_SPECIES_FILE_PATH);
        return loaded;
    }();
    return registry;
}

RunOutcome runSoak(std::uint64_t seed, int ticksToRun, int worldWidthInTiles,
                   int worldHeightInTiles) {
    HeadlessRunSettings settings;
    settings.worldSeed = seed;
    settings.ticksToRun = ticksToRun;
    settings.censusIntervalInTicks = 50;
    settings.worldWidthInTiles = worldWidthInTiles;
    settings.worldHeightInTiles = worldHeightInTiles;

    RunOutcome outcome;
    runHeadlessSimulation(settings, shippedSpeciesRegistry().allSpecies(),
                          [&outcome](const EcosystemCensus& census) {
                              for (std::size_t index = 0;
                                   index < static_cast<std::size_t>(kOrganismCategoryCount);
                                   ++index) {
                                  const int count = census.livingCountByCategory[index];
                                  if (outcome.censusCount == 0 ||
                                      count < outcome.lowestCountByCategory[index]) {
                                      outcome.lowestCountByCategory[index] = count;
                                  }
                              }
                              outcome.finalCensus = census;
                              ++outcome.censusCount;
                          });
    return outcome;
}

/// Returns the outcome for a default-world run of the given seed and length,
/// computing it once. Several cases below assert different things about the same
/// run, and a soak run is expensive enough that repeating one to ask a second
/// question of it doubles what the suite costs on every build.
const RunOutcome& memoizedDefaultWorldRun(std::uint64_t seed, int ticksToRun) {
    static std::map<std::pair<std::uint64_t, int>, RunOutcome> outcomeByRun;
    const auto key = std::pair{seed, ticksToRun};
    const auto existing = outcomeByRun.find(key);
    if (existing != outcomeByRun.end()) {
        return existing->second;
    }
    return outcomeByRun
        .emplace(key, runSoak(seed, ticksToRun, kDefaultWorldSize, kDefaultWorldSize))
        .first->second;
}

/// Names the seed and the category in the failure message, because a soak that
/// fails tells you nothing useful unless it says which level went and when.
void requireEveryTrophicLevelHeld(const RunOutcome& outcome, std::uint64_t seed) {
    for (std::size_t index = 0; index < static_cast<std::size_t>(kOrganismCategoryCount);
         ++index) {
        const auto category = static_cast<OrganismCategory>(index);
        INFO("seed " << seed << ": " << describeOrganismCategory(category) << " fell to "
                     << outcome.lowestCountByCategory[index]);
        REQUIRE(outcome.lowestCountByCategory[index] > 0);
    }
}

}

TEST_CASE("the starter ecosystem holds every trophic level", "[soak]") {
    // The regression this guards is the one the balance pass was opened for: three
    // of the five trophic levels used to be gone inside a couple of hundred ticks of
    // every seeded start, and nothing in the suite noticed, because every other test
    // asserts about one tick or one organism. Only a long run over several worlds
    // can tell a food web from a plant monoculture.
    for (const int seed : kQuickSeeds) {
        const RunOutcome& outcome =
            memoizedDefaultWorldRun(static_cast<std::uint64_t>(seed), kQuickTicks);
        requireEveryTrophicLevelHeld(outcome, static_cast<std::uint64_t>(seed));
    }
}

TEST_CASE("a soaked run stays a food web rather than a monoculture", "[soak]") {
    // Presence alone would pass on a world carrying two rabbits, which is a food web
    // about to collapse rather than one that is working. The pyramid has to still be
    // a pyramid at the end.
    const RunOutcome& outcome = memoizedDefaultWorldRun(1, kQuickTicks);
    const EcosystemCensus& ended = outcome.finalCensus;
    REQUIRE(ended.hasEveryTrophicCategory());
    REQUIRE(ended.livingCountOf(OrganismCategory::Plant) >
            ended.livingCountOf(OrganismCategory::Herbivore));
    REQUIRE(ended.livingCountOf(OrganismCategory::Herbivore) >
            ended.livingCountOf(OrganismCategory::Carnivore));
}

TEST_CASE("one seed reproduces one whole soak run", "[soak][determinism]") {
    // Determinism is what makes every number in this file mean anything: a soak
    // that drifted between builds could not distinguish a balance regression from
    // an unlucky run.
    const RunOutcome first =
        runSoak(7, kDeterminismTicks, kDefaultWorldSize, kDefaultWorldSize);
    const RunOutcome second =
        runSoak(7, kDeterminismTicks, kDefaultWorldSize, kDefaultWorldSize);
    REQUIRE(first.lowestCountByCategory == second.lowestCountByCategory);
    REQUIRE(formatCensusAsCsvRow(first.finalCensus) ==
            formatCensusAsCsvRow(second.finalCensus));
}

TEST_CASE("the starter ecosystem survives five thousand ticks on ten worlds",
          "[.soak-full]") {
    // The milestone's actual done-criterion, and far too slow to run on every build:
    // ten full-size worlds carrying tens of thousands of organisms for five thousand
    // ticks each. Hidden behind its own tag, run with `petriterm_tests [.soak-full]`
    // whenever a tuning constant or anything in species.txt changes.
    for (const int seed : kSoakSeeds) {
        const RunOutcome outcome = runSoak(static_cast<std::uint64_t>(seed), kSoakTicks,
                                           kDefaultWorldSize, kDefaultWorldSize);
        requireEveryTrophicLevelHeld(outcome, static_cast<std::uint64_t>(seed));
    }
}
