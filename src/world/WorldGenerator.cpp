#include "petriterm/world/WorldGenerator.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "petriterm/world/Biome.hpp"
#include "petriterm/world/NoiseGenerator.hpp"

namespace petriterm::world {

namespace {

constexpr double kNoiseFrequencyPerColumn = 0.045;
constexpr double kNoiseFrequencyPerRow = 0.09;
constexpr int kFractalOctaveCount = 4;
constexpr double kFractalPersistence = 0.5;

constexpr std::uint64_t kTemperatureSeedSalt = 0x9E3779B97F4A7C15ULL;
constexpr std::uint64_t kHumiditySeedSalt = 0xC2B2AE3D27D4EB4FULL;

constexpr double kMinimumTemperatureCelsius = -15.0;
constexpr double kMaximumTemperatureCelsius = 40.0;

constexpr double kTundraMaximumTemperatureCelsius = 3.0;
constexpr double kWetlandMinimumHumidityPercent = 65.0;
constexpr double kWetlandMaximumElevation = 0.35;
constexpr double kHotClimateMinimumTemperatureCelsius = 22.0;
constexpr double kJungleMinimumHumidityPercent = 55.0;
constexpr double kDesertMaximumHumidityPercent = 35.0;

/// Samples a widthInTiles x heightInTiles field of fractal noise in row-major
/// order, then stretches it linearly so its minimum maps to 0.0 and its maximum
/// to 1.0. The stretch guarantees every generated world spans the full climate
/// range, so no biome disappears on an unlucky seed.
std::vector<double> sampleNormalizedNoiseField(std::uint64_t fieldSeed, int widthInTiles,
                                               int heightInTiles) {
    const NoiseGenerator noiseGenerator(fieldSeed);
    std::vector<double> fieldValues;
    fieldValues.reserve(static_cast<std::size_t>(widthInTiles) *
                        static_cast<std::size_t>(heightInTiles));
    for (int rowIndex = 0; rowIndex < heightInTiles; ++rowIndex) {
        for (int columnIndex = 0; columnIndex < widthInTiles; ++columnIndex) {
            fieldValues.push_back(noiseGenerator.fractalNoiseAt(
                columnIndex * kNoiseFrequencyPerColumn, rowIndex * kNoiseFrequencyPerRow,
                kFractalOctaveCount, kFractalPersistence));
        }
    }

    const auto [minimumIt, maximumIt] =
        std::minmax_element(fieldValues.begin(), fieldValues.end());
    const double valueRange = *maximumIt - *minimumIt;
    if (valueRange > 0.0) {
        const double minimumValue = *minimumIt;
        for (double& value : fieldValues) {
            value = (value - minimumValue) / valueRange;
        }
    }
    return fieldValues;
}

}

BiomeType classifyBiomeFromClimate(double temperatureCelsius, double humidityPercent,
                                   double elevationNormalized) {
    if (temperatureCelsius < kTundraMaximumTemperatureCelsius) {
        return BiomeType::Tundra;
    }
    if (humidityPercent >= kWetlandMinimumHumidityPercent &&
        elevationNormalized < kWetlandMaximumElevation) {
        return BiomeType::Wetland;
    }
    if (temperatureCelsius >= kHotClimateMinimumTemperatureCelsius) {
        if (humidityPercent >= kJungleMinimumHumidityPercent) {
            return BiomeType::TropicalJungle;
        }
        if (humidityPercent < kDesertMaximumHumidityPercent) {
            return BiomeType::Desert;
        }
    }
    return BiomeType::Grassland;
}

WorldGrid generateWorld(int widthInTiles, int heightInTiles, std::uint64_t seed) {
    const std::vector<double> elevationField =
        sampleNormalizedNoiseField(seed, widthInTiles, heightInTiles);
    const std::vector<double> temperatureField = sampleNormalizedNoiseField(
        seed ^ kTemperatureSeedSalt, widthInTiles, heightInTiles);
    const std::vector<double> humidityField =
        sampleNormalizedNoiseField(seed ^ kHumiditySeedSalt, widthInTiles, heightInTiles);

    WorldGrid world(widthInTiles, heightInTiles);
    world.forEachTile([&](int columnIndex, int rowIndex, Tile& tile) {
        const std::size_t fieldIndex =
            static_cast<std::size_t>(rowIndex) * static_cast<std::size_t>(widthInTiles) +
            static_cast<std::size_t>(columnIndex);
        tile.elevationNormalized = elevationField[fieldIndex];
        tile.baseTemperatureCelsius =
            kMinimumTemperatureCelsius +
            temperatureField[fieldIndex] *
                (kMaximumTemperatureCelsius - kMinimumTemperatureCelsius);
        tile.baseHumidityPercent = humidityField[fieldIndex] * 100.0;
        tile.currentTemperatureCelsius = tile.baseTemperatureCelsius;
        tile.currentHumidityPercent = tile.baseHumidityPercent;
        tile.biome =
            classifyBiomeFromClimate(tile.baseTemperatureCelsius, tile.baseHumidityPercent,
                                     tile.elevationNormalized);
        tile.soilNutrientLevel = describeBiome(tile.biome).soilFertilityBaseline;
        tile.surfaceWaterLevel =
            (tile.baseHumidityPercent / 100.0) * (1.0 - tile.elevationNormalized);
    });
    return world;
}

}
