#include "petriterm/world/Tile.hpp"

#include "petriterm/organisms/Species.hpp"

namespace petriterm::world {

namespace {

/// Returns how many living organisms of the given category one tile can hold.
/// Producers pack densely; higher trophic levels are progressively sparser.
///
/// Grazers are given more room than the omnivores that compete with them and prey
/// on them both, which is the one place in this table where the trophic ordering is
/// not the whole story. A herd is the herbivore's answer to being eaten, and capping
/// it at the same density as its predator left the grazers with no way to hold a
/// population through a predator peak - they were the tier that kept going locally
/// extinct while everything above and below them persisted.
int perTileCapacityForCategory(organisms::OrganismCategory category) {
    switch (category) {
        case organisms::OrganismCategory::Plant:
            return 4;
        case organisms::OrganismCategory::Herbivore:
            return 3;
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
