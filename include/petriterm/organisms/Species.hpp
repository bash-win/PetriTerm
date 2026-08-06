#pragma once

#include <array>
#include <string>

#include "petriterm/engine/ColorPalette.hpp"
#include "petriterm/organisms/OrganismCategory.hpp"
#include "petriterm/organisms/TraitProfile.hpp"

namespace petriterm::organisms {

/// The set of organism categories a species can eat. Plants have an empty diet
/// (they photosynthesize instead); a herbivore's diet is {Plant}; a carnivore's
/// is {Herbivore, Omnivore}; an omnivore's is {Plant, Herbivore}.
class Diet {
public:
    /// Marks the given category as edible for this diet.
    void allowCategory(OrganismCategory category);

    /// Returns true if this diet includes the given category.
    bool canConsume(OrganismCategory category) const;

    /// Returns true if this diet includes at least one category.
    bool eatsAnything() const;

private:
    std::array<bool, kOrganismCategoryCount> edibleByCategory{};
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
