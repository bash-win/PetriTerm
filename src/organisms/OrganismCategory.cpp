#include "petriterm/organisms/OrganismCategory.hpp"

namespace petriterm::organisms {

std::string_view describeOrganismCategory(OrganismCategory category) {
    switch (category) {
        case OrganismCategory::Plant:
            return "Plant";
        case OrganismCategory::Herbivore:
            return "Herbivore";
        case OrganismCategory::Carnivore:
            return "Carnivore";
        case OrganismCategory::Omnivore:
            return "Omnivore";
        case OrganismCategory::Decomposer:
            return "Decomposer";
    }
    return "Plant";
}

std::optional<OrganismCategory> parseOrganismCategory(std::string_view text) {
    if (text == "Plant") {
        return OrganismCategory::Plant;
    }
    if (text == "Herbivore") {
        return OrganismCategory::Herbivore;
    }
    if (text == "Carnivore") {
        return OrganismCategory::Carnivore;
    }
    if (text == "Omnivore") {
        return OrganismCategory::Omnivore;
    }
    if (text == "Decomposer") {
        return OrganismCategory::Decomposer;
    }
    return std::nullopt;
}

}
