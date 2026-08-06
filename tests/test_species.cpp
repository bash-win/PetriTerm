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
