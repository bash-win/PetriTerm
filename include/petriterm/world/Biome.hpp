#pragma once

#include <string>

#include "petriterm/engine/ColorPalette.hpp"

namespace petriterm::world {

/// The five biome classes a tile can belong to, assigned by WorldGenerator
/// from the tile's temperature, humidity, and elevation.
enum class BiomeType {
    TropicalJungle,
    Desert,
    Grassland,
    Wetland,
    Tundra,
};

/// Immutable presentation and baseline-climate data for one biome type:
/// how its tiles are drawn and the climate band and soil fertility that
/// generation and simulation build on.
struct BiomeDescriptor {
    BiomeType type;
    std::string displayName;
    wchar_t backgroundGlyph;
    engine::TerminalColor backgroundColor;
    double baselineTemperatureCelsius;
    double baselineHumidityPercent;
    double soilFertilityBaseline;
};

/// Returns the immutable descriptor for the given biome type.
const BiomeDescriptor& describeBiome(BiomeType biome);

}
