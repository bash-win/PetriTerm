#include "petriterm/simulation/EcosystemCensus.hpp"

#include <cstddef>
#include <format>

#include "petriterm/world/Tile.hpp"
#include "petriterm/world/WorldGrid.hpp"

namespace petriterm::simulation {

namespace {

using organisms::kOrganismCategoryCount;
using organisms::OrganismCategory;

/// Decimal places for the fertility columns. Three is enough to watch soil drift
/// tick by tick without the column turning into noise from the last bits of the
/// double.
constexpr int kFertilityDecimalPlaces = 3;

}

int EcosystemCensus::livingCountOf(OrganismCategory category) const {
    return livingCountByCategory[static_cast<std::size_t>(category)];
}

double EcosystemCensus::meanSoilNutrientLevel() const {
    return tileCount > 0 ? totalSoilNutrientLevel / tileCount : 0.0;
}

double EcosystemCensus::meanDetritusLevel() const {
    return tileCount > 0 ? totalDetritusLevel / tileCount : 0.0;
}

double EcosystemCensus::meanCurrentTemperatureCelsius() const {
    return tileCount > 0 ? totalCurrentTemperatureCelsius / tileCount : 0.0;
}

bool EcosystemCensus::hasEveryTrophicCategory() const {
    for (int count : livingCountByCategory) {
        if (count <= 0) {
            return false;
        }
    }
    return true;
}

EcosystemCensus sampleEcosystemCensus(const SimulationEngine& engine) {
    const TickReport& report = engine.latestTickReport();

    EcosystemCensus census;
    census.tickIndex = engine.tickIndex();
    census.livingCountByCategory = report.livingCountByCategory;
    census.totalLivingCount = report.totalLivingCount;
    census.birthCount = report.birthCount;
    census.deathCount = report.deathCount;
    census.feedingCount = report.feedingCount;

    census.weatherPattern = engine.climate().currentWeatherPattern();
    census.season = engine.climate().currentSeason();

    engine.world().forEachTile([&census](int, int, const world::Tile& tile) {
        census.totalSoilNutrientLevel += tile.soilNutrientLevel;
        census.totalDetritusLevel += tile.detritusLevel;
        census.totalCurrentTemperatureCelsius += tile.currentTemperatureCelsius;
        ++census.tileCount;
    });
    return census;
}

std::string censusCsvHeader() {
    return "tick,plants,herbivores,carnivores,omnivores,decomposers,total,births,"
           "deaths,feedings,mean_soil,mean_detritus,mean_temp_c,season,weather";
}

std::string formatCensusAsCsvRow(const EcosystemCensus& census) {
    return std::format("{},{},{},{},{},{},{},{},{},{},{:.{}f},{:.{}f},{:.1f},{},{}",
                       census.tickIndex, census.livingCountOf(OrganismCategory::Plant),
                       census.livingCountOf(OrganismCategory::Herbivore),
                       census.livingCountOf(OrganismCategory::Carnivore),
                       census.livingCountOf(OrganismCategory::Omnivore),
                       census.livingCountOf(OrganismCategory::Decomposer),
                       census.totalLivingCount, census.birthCount, census.deathCount,
                       census.feedingCount, census.meanSoilNutrientLevel(),
                       kFertilityDecimalPlaces, census.meanDetritusLevel(),
                       kFertilityDecimalPlaces, census.meanCurrentTemperatureCelsius(),
                       world::describeSeason(census.season),
                       world::describeWeatherPattern(census.weatherPattern));
}

}
