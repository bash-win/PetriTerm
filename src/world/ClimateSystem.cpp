#include "petriterm/world/ClimateSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace petriterm::world {

namespace {

/// Presentation, climate offsets, duration band, and selection weight of one
/// weather pattern. Offsets are added to a tile's base climate while active.
struct WeatherPatternProfile {
    WeatherPattern pattern;
    std::string_view displayName;
    double temperatureDeltaCelsius;
    double humidityDeltaPercent;
    int minimumDurationTicks;
    int maximumDurationTicks;
    int selectionWeight;
};

/// Number of ticks in a full seasonal year; sets how fast the season phase
/// advances.
constexpr double kTicksPerSeasonalYear = 2000.0;

/// Peak warm/cold temperature swing at the height of summer/winter, in Celsius.
///
/// Small next to the tolerance bands in species.txt on purpose. The season lasts
/// five hundred ticks, which is longer than any organism's energy reserve can
/// outlast a shortfall, so a seasonal offset is not something a population can ride
/// out the way it rides out a heatwave - whatever the season does to fitness, it
/// does for long enough to be decided by it. This is the offset that biases a year;
/// the drama belongs to the weather patterns below.
constexpr double kSeasonalTemperatureAmplitudeCelsius = 4.0;

/// Ticks the initial Clear pattern lasts before the first weather transition.
constexpr int kInitialPatternDurationTicks = 60;

/// Every weather pattern, in WeatherPattern order.
///
/// Offset and duration are tuned against each other rather than independently,
/// because whether a pattern is weather or an extinction event is decided by the
/// pair. An organism carries at most a couple of dozen ticks of upkeep in reserve,
/// and a pattern that holds it below the fitness it needs to cover that upkeep for
/// longer than the reserve lasts does not stress a population, it removes one. So
/// the two sharp patterns are the two short ones: a heatwave is a scare that costs
/// a plant most of its stored energy and stops it seeding, and it is over before
/// the reserve is. The mild patterns are the ones allowed to last.
const std::array<WeatherPatternProfile, 5>& weatherPatternProfiles() {
    static constexpr std::array<WeatherPatternProfile, 5> profiles{{
        {WeatherPattern::Clear, "Clear", 0.0, 0.0, 40, 90, 50},
        {WeatherPattern::Rainy, "Rainy", -2.0, 12.0, 25, 60, 20},
        {WeatherPattern::Heatwave, "Heatwave", 5.0, -8.0, 10, 20, 10},
        {WeatherPattern::ColdSnap, "Cold Snap", -6.0, -3.0, 10, 20, 10},
        {WeatherPattern::Storm, "Storm", -3.0, 16.0, 8, 16, 10},
    }};
    return profiles;
}

const WeatherPatternProfile& profileFor(WeatherPattern pattern) {
    return weatherPatternProfiles()[static_cast<std::size_t>(pattern)];
}

}

std::string_view describeWeatherPattern(WeatherPattern pattern) {
    return profileFor(pattern).displayName;
}

std::string_view describeSeason(Season season) {
    switch (season) {
        case Season::Spring:
            return "Spring";
        case Season::Summer:
            return "Summer";
        case Season::Autumn:
            return "Autumn";
        case Season::Winter:
            return "Winter";
    }
    return "Spring";
}

ClimateSystem::ClimateSystem(engine::RandomNumberGenerator& sharedRandom)
    : sharedRandom(sharedRandom),
      activePattern(WeatherPattern::Clear),
      remainingPatternTicks(kInitialPatternDurationTicks),
      seasonPhaseNormalized(0.0) {}

Season ClimateSystem::currentSeason() const {
    const int seasonIndex = static_cast<int>(seasonPhaseNormalized * 4.0);
    return static_cast<Season>(std::clamp(seasonIndex, 0, 3));
}

double ClimateSystem::seasonalTemperatureOffsetCelsius() const {
    return kSeasonalTemperatureAmplitudeCelsius *
           std::sin(2.0 * std::numbers::pi * seasonPhaseNormalized);
}

void ClimateSystem::transitionToNextPattern() {
    const auto& profiles = weatherPatternProfiles();
    int totalWeight = 0;
    for (const auto& profile : profiles) {
        totalWeight += profile.selectionWeight;
    }
    int weightedRoll = sharedRandom.integerInRange(1, totalWeight);
    for (const auto& profile : profiles) {
        weightedRoll -= profile.selectionWeight;
        if (weightedRoll <= 0) {
            activePattern = profile.pattern;
            break;
        }
    }
    const auto& chosen = profileFor(activePattern);
    remainingPatternTicks = sharedRandom.integerInRange(chosen.minimumDurationTicks,
                                                        chosen.maximumDurationTicks);
}

void ClimateSystem::applyClimateToWorld(WorldGrid& world) const {
    const double seasonalOffset = seasonalTemperatureOffsetCelsius();
    const WeatherPatternProfile& profile = profileFor(activePattern);
    world.forEachTile([&](int, int, Tile& tile) {
        tile.currentTemperatureCelsius =
            tile.baseTemperatureCelsius + seasonalOffset + profile.temperatureDeltaCelsius;
        tile.currentHumidityPercent =
            std::clamp(tile.baseHumidityPercent + profile.humidityDeltaPercent, 0.0, 100.0);
    });
}

void ClimateSystem::advanceWeatherAndApplyToWorld(WorldGrid& world) {
    seasonPhaseNormalized += 1.0 / kTicksPerSeasonalYear;
    if (seasonPhaseNormalized >= 1.0) {
        seasonPhaseNormalized -= 1.0;
    }
    --remainingPatternTicks;
    if (remainingPatternTicks <= 0) {
        transitionToNextPattern();
    }
    applyClimateToWorld(world);
}

}
