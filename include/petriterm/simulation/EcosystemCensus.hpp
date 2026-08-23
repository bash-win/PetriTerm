#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "petriterm/organisms/OrganismCategory.hpp"
#include "petriterm/simulation/SimulationEngine.hpp"
#include "petriterm/world/ClimateSystem.hpp"

namespace petriterm::simulation {

/// One tick's worth of everything a tuning run needs to see, sampled after the
/// tick has been applied.
///
/// This is deliberately wider than TickReport. A population column alone hides
/// both of the ways the nutrient cycle goes wrong: a plot can read as healthy
/// right up to the tick its soil runs out, and detritus can pile up untouched
/// because the decomposers starved before anything died. Fertility has to be in
/// the output for either to be visible while it is happening rather than after
/// the collapse.
struct EcosystemCensus {
    std::uint64_t tickIndex = 0;
    std::array<int, organisms::kOrganismCategoryCount> livingCountByCategory{};
    int totalLivingCount = 0;
    int birthCount = 0;
    int deathCount = 0;
    int feedingCount = 0;

    /// Summed across every tile, so these grow with the world; the per-tile means
    /// below are what to compare between runs of different sizes.
    double totalSoilNutrientLevel = 0.0;
    double totalDetritusLevel = 0.0;
    double totalCurrentTemperatureCelsius = 0.0;
    int tileCount = 0;

    /// The weather and season in force on the sampled tick.
    ///
    /// Present because a population crash has two very different causes that look
    /// identical in a population column: the food web failing on its own terms, or
    /// a cold snap landing on top of a hard winter and putting most of the map
    /// outside every species' tolerance band at once. Telling those apart from the
    /// output is the difference between tuning the right number and the wrong one.
    world::WeatherPattern weatherPattern = world::WeatherPattern::Clear;
    world::Season season = world::Season::Spring;

    /// Returns the living population of the given category.
    int livingCountOf(organisms::OrganismCategory category) const;

    /// Returns fertility per tile, or zero for an empty world.
    double meanSoilNutrientLevel() const;

    /// Returns detritus per tile, or zero for an empty world.
    double meanDetritusLevel() const;

    /// Returns the mean current temperature across the world, or zero for an empty
    /// one. The single number that says whether the map as a whole is somewhere
    /// its inhabitants can live this tick.
    double meanCurrentTemperatureCelsius() const;

    /// Returns true if every trophic category still has at least one member. The
    /// soak test's definition of a food web that is still standing: losing any one
    /// category means the levels above it are living on borrowed time.
    bool hasEveryTrophicCategory() const;
};

/// Samples the engine's current state. Reads the engine's own latest tick report
/// for the population and event counts rather than recounting them, so the census
/// cannot disagree with what the tick actually did, and walks the world once for
/// the fertility totals.
EcosystemCensus sampleEcosystemCensus(const SimulationEngine& engine);

/// Returns the CSV header naming every column formatCensusAsCsvRow emits, without
/// a trailing newline.
std::string censusCsvHeader();

/// Returns one CSV row, without a trailing newline. Fertility is written as the
/// per-tile mean and at a fixed precision, so a run's output diffs cleanly
/// against another run's.
std::string formatCensusAsCsvRow(const EcosystemCensus& census);

}
