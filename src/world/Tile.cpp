#include "petriterm/world/Tile.hpp"

#include "petriterm/organisms/Species.hpp"

namespace petriterm::world {

namespace {

/// Returns how many living organisms of the given category one tile can hold.
/// Producers pack densely; higher trophic levels are progressively sparser.
int perTileCapacityForCategory(organisms::OrganismCategory category) {
    switch (category) {
        case organisms::OrganismCategory::Plant:
            return 4;
        case organisms::OrganismCategory::Herbivore:
            return 2;
        case organisms::OrganismCategory::Carnivore:
            return 1;
        case organisms::OrganismCategory::Omnivore:
            return 2;
        case organisms::OrganismCategory::Decomposer:
            return 3;
    }
    return 1;
}

}

int Tile::livingOrganismCount() const {
    int livingCount = 0;
    for (const auto& organism : occupyingOrganisms) {
        if (organism->isAlive) {
            ++livingCount;
        }
    }
    return livingCount;
}

bool Tile::hasCapacityForCategory(organisms::OrganismCategory category) const {
    int livingInCategory = 0;
    for (const auto& organism : occupyingOrganisms) {
        if (organism->isAlive && organism->species->category == category) {
            ++livingInCategory;
        }
    }
    return livingInCategory < perTileCapacityForCategory(category);
}

}
