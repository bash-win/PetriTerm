#pragma once

#include <string_view>

#include "petriterm/engine/RandomNumberGenerator.hpp"
#include "petriterm/world/WorldGrid.hpp"

namespace petriterm::world {

/// The active large-scale weather regime, which shifts every tile's climate away
/// from its baseline while in effect.
enum class WeatherPattern {
    Clear,
    Rainy,
    Heatwave,
    ColdSnap,
    Storm,
};

/// The slowly cycling time of year, which biases temperature warm or cold.
enum class Season {
    Spring,
    Summer,
    Autumn,
    Winter,
};

/// Returns the display name of the given weather pattern.
std::string_view describeWeatherPattern(WeatherPattern pattern);

/// Returns the display name of the given season.
std::string_view describeSeason(Season season);

/// Drives live weather over the world. Holds a slowly advancing season phase and
/// a current weather pattern with a remaining duration. Each tick it advances the
/// season, possibly transitions the weather pattern, and re-derives every tile's
/// current temperature and humidity from its baseline plus the season and pattern
/// offsets. Weather transitions draw from the shared RNG, so a fixed seed
/// reproduces the same weather sequence.
class ClimateSystem {
public:
    /// Constructs the climate system using the given RNG for weather transitions.
    /// The RNG is owned externally by the simulator and shared with it.
    explicit ClimateSystem(engine::RandomNumberGenerator& sharedRandom);

    /// Advances weather by one tick: advances the season phase, counts down and
    /// possibly transitions the active weather pattern, then updates every tile's
    /// current temperature and humidity around its base values.
    void advanceWeatherAndApplyToWorld(WorldGrid& world);

    /// Returns the currently active weather pattern for HUD display.
    WeatherPattern currentWeatherPattern() const { return activePattern; }

    /// Returns the current season for HUD display.
    Season currentSeason() const;

private:
    void transitionToNextPattern();
    void applyClimateToWorld(WorldGrid& world) const;
    double seasonalTemperatureOffsetCelsius() const;

    engine::RandomNumberGenerator& sharedRandom;
    WeatherPattern activePattern;
    int remainingPatternTicks;
    double seasonPhaseNormalized;
};

}
