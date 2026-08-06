#pragma once

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "petriterm/organisms/Species.hpp"

namespace petriterm::organisms {

/// Thrown when the species file cannot be opened or contains a malformed line.
/// Carries the 1-based line number of the offending line (0 for whole-file
/// errors such as a missing file).
class SpeciesFileParseError : public std::runtime_error {
public:
    SpeciesFileParseError(int lineNumber, const std::string& message);

    int lineNumber() const { return offendingLineNumber; }

private:
    int offendingLineNumber;
};

/// Loads species definitions from the data file, owns them for the program's
/// lifetime, and hands out stable const pointers into that storage.
class SpeciesRegistry {
public:
    /// Parses the species definition file and populates the registry, replacing
    /// any previously loaded species. Throws SpeciesFileParseError with a line
    /// number on malformed input.
    void loadFromFile(const std::filesystem::path& speciesFilePath);

    /// Returns a stable pointer to the species with the given id, or nullptr if
    /// no such species is registered.
    const Species* findSpeciesById(std::string_view speciesId) const;

    /// Returns pointers to all registered species in file order, used to build
    /// the placement palette.
    std::vector<const Species*> allSpecies() const;

private:
    std::vector<std::unique_ptr<Species>> ownedSpecies;
    std::unordered_map<std::string, const Species*> speciesById;
};

}
