#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>

#include "petriterm/organisms/OrganismCategory.hpp"
#include "petriterm/organisms/SpeciesRegistry.hpp"

using petriterm::organisms::OrganismCategory;
using petriterm::organisms::Species;
using petriterm::organisms::SpeciesFileParseError;
using petriterm::organisms::SpeciesRegistry;

namespace {

std::filesystem::path writeTemporarySpeciesFile(std::string_view fileTag,
                                                std::string_view contents) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("petriterm_species_" + std::string(fileTag) + ".txt");
    std::ofstream output(path, std::ios::trunc);
    output << contents;
    output.close();
    return path;
}

constexpr std::string_view kValidPlantAndHerbivore =
    "# a comment\n"
    "\n"
    "id=moss;name=Green Moss;category=Plant;glyph=m;color=Green;cost=2;idealTemp=15;"
    "tempTol=8;idealHumidity=70;humidityTol=20;energyGain=4;energyPerTick=1;"
    "energyToReproduce=10;reproCost=5;reproCooldown=5;maxAge=100;moveRange=0;"
    "seedProb=0.2;diet=\n"
    "id=hare;name=Snow Hare;category=Herbivore;glyph=h;color=White;cost=5;idealTemp=5;"
    "tempTol=12;idealHumidity=50;humidityTol=30;energyGain=8;energyPerTick=2;"
    "energyToReproduce=20;reproCost=10;reproCooldown=8;maxAge=120;moveRange=2;"
    "seedProb=0;diet=Plant\n";

}

TEST_CASE("SpeciesRegistry loads valid species and skips comments and blanks",
          "[registry]") {
    const auto path = writeTemporarySpeciesFile("valid", kValidPlantAndHerbivore);
    SpeciesRegistry registry;
    registry.loadFromFile(path);

    REQUIRE(registry.allSpecies().size() == 2);
    REQUIRE(registry.allSpecies()[0]->speciesId == "moss");
    REQUIRE(registry.allSpecies()[1]->speciesId == "hare");

    const Species* hare = registry.findSpeciesById("hare");
    REQUIRE(hare != nullptr);
    REQUIRE(hare->displayName == "Snow Hare");
    REQUIRE(hare->category == OrganismCategory::Herbivore);
    REQUIRE(hare->traits.movementRangeInTiles == 2);
    REQUIRE(hare->traits.idealTemperatureCelsius == 5.0);
    REQUIRE(hare->canConsumeCategory(OrganismCategory::Plant));
    REQUIRE_FALSE(hare->canConsumeCategory(OrganismCategory::Herbivore));

    const Species* moss = registry.findSpeciesById("moss");
    REQUIRE(moss != nullptr);
    REQUIRE_FALSE(moss->diet.eatsAnything());
    REQUIRE(moss->glyphColor == petriterm::engine::TerminalColor::Green);
}

TEST_CASE("findSpeciesById returns nullptr for an unknown id", "[registry]") {
    const auto path = writeTemporarySpeciesFile("unknown", kValidPlantAndHerbivore);
    SpeciesRegistry registry;
    registry.loadFromFile(path);
    REQUIRE(registry.findSpeciesById("dragon") == nullptr);
}

TEST_CASE("a malformed line throws with its line number", "[registry]") {
    const std::string contents =
        "# header\n"
        "\n"
        "id=moss;name=Green Moss;category=Plant;glyph=m;color=Green;cost=2;idealTemp=15;"
        "tempTol=8;idealHumidity=70;humidityTol=20;energyGain=4;energyPerTick=1;"
        "energyToReproduce=10;reproCost=5;reproCooldown=5;maxAge=100;moveRange=0;"
        "seedProb=0.2;diet=\n"
        "id=broken;name=Broken;category=Plant;glyph=b;color=Green;cost=notanumber\n";
    const auto path = writeTemporarySpeciesFile("malformed", contents);
    SpeciesRegistry registry;
    try {
        registry.loadFromFile(path);
        FAIL("expected SpeciesFileParseError");
    } catch (const SpeciesFileParseError& error) {
        REQUIRE(error.lineNumber() == 4);
    }
}

TEST_CASE("parse errors are reported for each kind of malformed input", "[registry]") {
    const std::string prefix =
        "id=x;name=X;category=Plant;glyph=x;color=Green;cost=1;idealTemp=1;tempTol=1;"
        "idealHumidity=1;humidityTol=1;energyGain=1;energyPerTick=1;energyToReproduce=1;"
        "reproCost=1;reproCooldown=1;maxAge=1;moveRange=1;seedProb=1;";

    SECTION("missing field") {
        const auto path =
            writeTemporarySpeciesFile("missing", "id=x;name=X;category=Plant\n");
        SpeciesRegistry registry;
        REQUIRE_THROWS_AS(registry.loadFromFile(path), SpeciesFileParseError);
    }
    SECTION("unknown category") {
        const std::string contents =
            "id=x;name=X;category=Fungus;glyph=x;color=Green;cost=1;idealTemp=1;tempTol=1;"
            "idealHumidity=1;humidityTol=1;energyGain=1;energyPerTick=1;energyToReproduce="
            "1;"
            "reproCost=1;reproCooldown=1;maxAge=1;moveRange=1;seedProb=1;diet=\n";
        const auto path = writeTemporarySpeciesFile("badcat", contents);
        SpeciesRegistry registry;
        REQUIRE_THROWS_AS(registry.loadFromFile(path), SpeciesFileParseError);
    }
    SECTION("unknown diet category") {
        const auto path =
            writeTemporarySpeciesFile("baddiet", prefix + "diet=Plant,Alien\n");
        SpeciesRegistry registry;
        REQUIRE_THROWS_AS(registry.loadFromFile(path), SpeciesFileParseError);
    }
    SECTION("duplicate id") {
        const auto path =
            writeTemporarySpeciesFile("dup", prefix + "diet=\n" + prefix + "diet=\n");
        SpeciesRegistry registry;
        REQUIRE_THROWS_AS(registry.loadFromFile(path), SpeciesFileParseError);
    }
}

TEST_CASE("opening a missing file throws", "[registry]") {
    SpeciesRegistry registry;
    REQUIRE_THROWS_AS(registry.loadFromFile("/no/such/petriterm/species.txt"),
                      SpeciesFileParseError);
}

TEST_CASE("the shipped species file loads and forms a food web", "[registry]") {
    SpeciesRegistry registry;
    registry.loadFromFile(PETRITERM_SPECIES_FILE_PATH);

    std::map<OrganismCategory, int> countByCategory;
    for (const Species* species : registry.allSpecies()) {
        ++countByCategory[species->category];
    }
    REQUIRE(countByCategory[OrganismCategory::Plant] >= 5);
    REQUIRE(countByCategory[OrganismCategory::Herbivore] >= 2);
    REQUIRE(countByCategory[OrganismCategory::Carnivore] >= 2);
    REQUIRE(countByCategory[OrganismCategory::Omnivore] >= 1);
    REQUIRE(countByCategory[OrganismCategory::Decomposer] >= 1);

    const Species* fox = registry.findSpeciesById("fox");
    REQUIRE(fox != nullptr);
    REQUIRE(fox->category == OrganismCategory::Carnivore);
    REQUIRE(fox->canConsumeCategory(OrganismCategory::Herbivore));
    REQUIRE_FALSE(fox->canConsumeCategory(OrganismCategory::Plant));

    const Species* fern = registry.findSpeciesById("jungle_fern");
    REQUIRE(fern != nullptr);
    REQUIRE_FALSE(fern->diet.eatsAnything());
}
