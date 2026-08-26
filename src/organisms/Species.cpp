#include "petriterm/organisms/Species.hpp"

namespace petriterm::organisms {

void Diet::allowCategory(OrganismCategory category) {
    // Ignoring a repeat rather than appending it keeps the count bounded by the
    // number of categories, which is what lets the storage be a fixed array, and
    // stops a species file listing the same category twice from making that
    // category likelier to be chosen than the one after it.
    if (canConsume(category) || allowedCategoryCount >= orderedCategories.size()) {
        return;
    }
    orderedCategories[allowedCategoryCount] = category;
    ++allowedCategoryCount;
}

bool Diet::canConsume(OrganismCategory category) const {
    for (const OrganismCategory allowed : categoriesInPreferenceOrder()) {
        if (allowed == category) {
            return true;
        }
    }
    return false;
}

bool Diet::eatsAnything() const {
    return allowedCategoryCount > 0;
}

std::span<const OrganismCategory> Diet::categoriesInPreferenceOrder() const {
    return {orderedCategories.data(), allowedCategoryCount};
}

bool Species::canConsumeCategory(OrganismCategory preyCategory) const {
    return diet.canConsume(preyCategory);
}

}
