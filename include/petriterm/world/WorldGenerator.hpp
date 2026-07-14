#pragma once

#include <cstdint>

#include "petriterm/world/WorldGrid.hpp"

namespace petriterm::world {

inline constexpr int kDefaultWorldWidthInTiles = 160;
inline constexpr int kDefaultWorldHeightInTiles = 48;

/// Classifies a tile into a biome from its climate: freezing temperatures make
/// Tundra; humid lowlands make Wetland; hot climates split into TropicalJungle
/// (humid) and Desert (dry); everything moderate is Grassland. Pure function;
/// the core of biome generation.
BiomeType classifyBiomeFromClimate(double temperatureCelsius, double humidityPercent,
                                   double elevationNormalized);

/// Generates and returns a complete world of the given size: elevation,
/// temperature, and humidity are sampled from three decorrelated fractal noise
/// fields (each stretched to cover its full range so every biome can appear),
/// each tile is classified into a biome, and soil and surface water baselines
/// are set. The same seed and size always yield an identical world.
WorldGrid generateWorld(int widthInTiles, int heightInTiles, std::uint64_t seed);

}
