#pragma once

#include <memory>
#include <vector>

#include "petriterm/organisms/Organism.hpp"
#include "petriterm/organisms/OrganismCategory.hpp"
#include "petriterm/world/Biome.hpp"

namespace petriterm::world {

/// The atomic unit of the world: one grid cell's biome, terrain, climate state,
/// and the organisms occupying it. Base values are fixed at generation; current
/// values are re-derived from them each tick by the climate system. Owns its
/// organisms via unique_ptr, so a Tile is movable but not copyable.
struct Tile {
    BiomeType biome = BiomeType::Grassland;
    double elevationNormalized = 0.0;
    double baseTemperatureCelsius = 0.0;
    double baseHumidityPercent = 0.0;
    double currentTemperatureCelsius = 0.0;
    double currentHumidityPercent = 0.0;
    double soilNutrientLevel = 0.0;
    double surfaceWaterLevel = 0.0;
    std::vector<std::unique_ptr<organisms::Organism>> occupyingOrganisms;

    /// Returns the number of living organisms currently on this tile.
    int livingOrganismCount() const;

    /// Returns true if the tile has room for another organism of the given
    /// category, given the per-tile carrying capacity for that category.
    bool hasCapacityForCategory(organisms::OrganismCategory category) const;
};

}
