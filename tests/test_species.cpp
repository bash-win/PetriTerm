#include <catch2/catch_test_macros.hpp>

#include "petriterm/organisms/OrganismCategory.hpp"
#include "petriterm/organisms/Species.hpp"

using petriterm::organisms::describeOrganismCategory;
using petriterm::organisms::Diet;
using petriterm::organisms::OrganismCategory;
using petriterm::organisms::parseOrganismCategory;
using petriterm::organisms::Species;

namespace {

constexpr OrganismCategory kAllCategories[] = {
    OrganismCategory::Plant, OrganismCategory::Herbivore, OrganismCategory::Carnivore,
    OrganismCategory::Omnivore, OrganismCategory::Decomposer};

}

TEST_CASE("parseOrganismCategory round-trips every category name", "[species]") {
    for (const OrganismCategory category : kAllCategories) {
        const auto parsed = parseOrganismCategory(describeOrganismCategory(category));
        REQUIRE(parsed.has_value());
        REQUIRE(parsed.value() == category);
    }
}

TEST_CASE("parseOrganismCategory rejects unknown names", "[species]") {
    REQUIRE_FALSE(parseOrganismCategory("").has_value());
    REQUIRE_FALSE(parseOrganismCategory("Fungus").has_value());
    REQUIRE_FALSE(parseOrganismCategory("plant").has_value());
}

TEST_CASE("an empty diet consumes nothing", "[species]") {
    const Diet diet;
    REQUIRE_FALSE(diet.eatsAnything());
    for (const OrganismCategory category : kAllCategories) {
        REQUIRE_FALSE(diet.canConsume(category));
    }
}

TEST_CASE("a diet consumes exactly the categories it allows", "[species]") {
    Diet diet;
    diet.allowCategory(OrganismCategory::Herbivore);
    diet.allowCategory(OrganismCategory::Omnivore);
    REQUIRE(diet.eatsAnything());
    REQUIRE(diet.canConsume(OrganismCategory::Herbivore));
    REQUIRE(diet.canConsume(OrganismCategory::Omnivore));
    REQUIRE_FALSE(diet.canConsume(OrganismCategory::Plant));
    REQUIRE_FALSE(diet.canConsume(OrganismCategory::Carnivore));
}

TEST_CASE("a diet keeps the order its categories were added in", "[species]") {
    Diet diet;
    diet.allowCategory(OrganismCategory::Plant);
    diet.allowCategory(OrganismCategory::Herbivore);

    const auto ordered = diet.categoriesInPreferenceOrder();
    REQUIRE(ordered.size() == 2);
    REQUIRE(ordered[0] == OrganismCategory::Plant);
    REQUIRE(ordered[1] == OrganismCategory::Herbivore);
}

TEST_CASE("a repeated diet category does not take a second slot", "[species]") {
    // A species file listing a category twice would otherwise make it two of the
    // entries the engine walks, and so likelier to be chosen than the one after it.
    Diet diet;
    diet.allowCategory(OrganismCategory::Herbivore);
    diet.allowCategory(OrganismCategory::Herbivore);
    diet.allowCategory(OrganismCategory::Omnivore);

    const auto ordered = diet.categoriesInPreferenceOrder();
    REQUIRE(ordered.size() == 2);
    REQUIRE(ordered[0] == OrganismCategory::Herbivore);
    REQUIRE(ordered[1] == OrganismCategory::Omnivore);
}

TEST_CASE("an empty diet offers nothing to walk", "[species]") {
    const Diet diet;
    REQUIRE(diet.categoriesInPreferenceOrder().empty());
}

TEST_CASE("Species canConsumeCategory reflects its diet", "[species]") {
    Species herbivore;
    herbivore.speciesId = "rabbit";
    herbivore.category = OrganismCategory::Herbivore;
    herbivore.diet.allowCategory(OrganismCategory::Plant);
    REQUIRE(herbivore.canConsumeCategory(OrganismCategory::Plant));
    REQUIRE_FALSE(herbivore.canConsumeCategory(OrganismCategory::Herbivore));

    const Species plant;
    for (const OrganismCategory category : kAllCategories) {
        REQUIRE_FALSE(plant.canConsumeCategory(category));
    }
}
