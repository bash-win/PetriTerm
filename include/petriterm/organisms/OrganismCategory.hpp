#pragma once

#include <optional>
#include <string_view>

namespace petriterm::organisms {

/// The trophic role of a species, which selects the behavior branch the
/// simulator runs for it and the per-tile carrying-capacity bucket it occupies.
enum class OrganismCategory {
    Plant,
    Herbivore,
    Carnivore,
    Omnivore,
    Decomposer,
};

/// The number of organism categories, for sizing category-indexed containers.
inline constexpr int kOrganismCategoryCount = 5;

/// Returns the display name of the given category.
std::string_view describeOrganismCategory(OrganismCategory category);

/// Parses a category from its display name (as written in the species file),
/// returning std::nullopt if the text matches no category.
std::optional<OrganismCategory> parseOrganismCategory(std::string_view text);

}
