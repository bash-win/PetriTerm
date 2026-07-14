#pragma once

#include "petriterm/world/Biome.hpp"

namespace petriterm::world {

/// The atomic unit of the world: one grid cell's biome, terrain, and climate
/// state. Base values are fixed at generation; current values are re-derived
/// from them each tick by the climate system. The list of occupying organisms
/// and its capacity queries join this type with the organisms layer.
struct Tile {
    BiomeType biome = BiomeType::Grassland;
    double elevationNormalized = 0.0;
    double baseTemperatureCelsius = 0.0;
    double baseHumidityPercent = 0.0;
    double currentTemperatureCelsius = 0.0;
    double currentHumidityPercent = 0.0;
    double soilNutrientLevel = 0.0;
    double surfaceWaterLevel = 0.0;
};

}
