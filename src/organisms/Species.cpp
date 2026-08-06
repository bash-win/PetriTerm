#include "petriterm/organisms/Species.hpp"

#include <cstddef>

namespace petriterm::organisms {

void Diet::allowCategory(OrganismCategory category) {
    edibleByCategory[static_cast<std::size_t>(category)] = true;
}

bool Diet::canConsume(OrganismCategory category) const {
    return edibleByCategory[static_cast<std::size_t>(category)];
}

bool Diet::eatsAnything() const {
    for (const bool isEdible : edibleByCategory) {
        if (isEdible) {
            return true;
        }
    }
    return false;
}

bool Species::canConsumeCategory(OrganismCategory preyCategory) const {
    return diet.canConsume(preyCategory);
}

}
