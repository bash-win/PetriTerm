#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string>

#include "petriterm/engine/ColorPalette.hpp"
#include "petriterm/organisms/OrganismCategory.hpp"
#include "petriterm/organisms/TraitProfile.hpp"

namespace petriterm::organisms {

/// The organism categories a species eats, in preference order. Plants have an
/// empty diet (they photosynthesize instead); a herbivore's diet is {Plant}; a
/// carnivore's is {Herbivore, Omnivore}; an omnivore's is {Plant, Herbivore}.
///
/// Ordered rather than a plain set, because for anything eating more than one
/// category the order is the difference between an omnivore and a super-predator.
/// Given a free choice a boar that hunts every rabbit it passes while also grazing
/// is not competing with the herbivores, it is farming them: it is never short of
/// plants, so nothing ever limits how many rabbits it takes, and the grazers go
/// extinct underneath it. Listing plants first makes it an opportunist that takes
/// the easy food in reach and only hunts when there is none - which is both what a
/// boar does and what leaves the tier below it able to persist.
class Diet {
public:
    /// Adds the category to the diet if it is not already in it, keeping it after
    /// everything added before it. The species file's listing order is therefore
    /// the preference order.
    void allowCategory(OrganismCategory category);

    /// Returns true if this diet includes the given category.
    bool canConsume(OrganismCategory category) const;

    /// Returns true if this diet includes at least one category.
    bool eatsAnything() const;

    /// The categories this diet allows, most preferred first.
    std::span<const OrganismCategory> categoriesInPreferenceOrder() const;

private:
    std::array<OrganismCategory, kOrganismCategoryCount> orderedCategories{};
    std::size_t allowedCategoryCount = 0;
};

/// Immutable definition of one species: identity, trophic category, tuning
/// traits, diet, and presentation. Owned by the SpeciesRegistry and referenced
/// elsewhere by stable const pointer for the program's lifetime.
struct Species {
    std::string speciesId;
    std::string displayName;
    OrganismCategory category = OrganismCategory::Plant;
    TraitProfile traits;
    Diet diet;
    wchar_t glyph = L'?';
    engine::TerminalColor glyphColor = engine::TerminalColor::White;
    int ecoCreditCostToPlace = 0;

    /// Returns true if this species can eat organisms of the given category,
    /// per its diet.
    bool canConsumeCategory(OrganismCategory preyCategory) const;
};

}
