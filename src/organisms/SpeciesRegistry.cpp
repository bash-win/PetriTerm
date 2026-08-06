#include "petriterm/organisms/SpeciesRegistry.hpp"

#include <charconv>
#include <fstream>
#include <optional>
#include <utility>

namespace petriterm::organisms {

namespace {

std::string formatParseError(int lineNumber, const std::string& message) {
    if (lineNumber > 0) {
        return "species file parse error at line " + std::to_string(lineNumber) + ": " +
               message;
    }
    return "species file error: " + message;
}

std::string_view leftTrimmed(std::string_view text) {
    std::size_t start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) {
        ++start;
    }
    return text.substr(start);
}

std::string_view rightTrimmed(std::string_view text) {
    std::size_t end = text.size();
    while (end > 0 && (text[end - 1] == ' ' || text[end - 1] == '\t')) {
        --end;
    }
    return text.substr(0, end);
}

std::optional<double> parseDouble(std::string_view text) {
    double value = 0.0;
    const auto* const begin = text.data();
    const auto* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc() || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

std::optional<int> parseInt(std::string_view text) {
    int value = 0;
    const auto* const begin = text.data();
    const auto* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc() || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

std::optional<engine::TerminalColor> parseColor(std::string_view text) {
    using engine::TerminalColor;
    if (text == "Default") {
        return TerminalColor::Default;
    }
    if (text == "Black") {
        return TerminalColor::Black;
    }
    if (text == "Red") {
        return TerminalColor::Red;
    }
    if (text == "Green") {
        return TerminalColor::Green;
    }
    if (text == "Yellow") {
        return TerminalColor::Yellow;
    }
    if (text == "Blue") {
        return TerminalColor::Blue;
    }
    if (text == "Magenta") {
        return TerminalColor::Magenta;
    }
    if (text == "Cyan") {
        return TerminalColor::Cyan;
    }
    if (text == "White") {
        return TerminalColor::White;
    }
    return std::nullopt;
}

/// Decodes the first UTF-8 code point of the text into a wide character,
/// returning 0 for empty input. Bytes that are not a valid lead byte are
/// returned as-is.
wchar_t decodeFirstCodePoint(std::string_view text) {
    if (text.empty()) {
        return 0;
    }
    const auto leadByte = static_cast<unsigned char>(text[0]);
    int continuationCount = 0;
    char32_t codePoint = 0;
    if (leadByte < 0x80) {
        return static_cast<wchar_t>(leadByte);
    }
    if ((leadByte >> 5) == 0x6) {
        continuationCount = 1;
        codePoint = leadByte & 0x1F;
    } else if ((leadByte >> 4) == 0xE) {
        continuationCount = 2;
        codePoint = leadByte & 0x0F;
    } else if ((leadByte >> 3) == 0x1E) {
        continuationCount = 3;
        codePoint = leadByte & 0x07;
    } else {
        return static_cast<wchar_t>(leadByte);
    }
    for (int index = 1; index <= continuationCount && index < static_cast<int>(text.size());
         ++index) {
        codePoint = (codePoint << 6) | (static_cast<unsigned char>(text[index]) & 0x3F);
    }
    return static_cast<wchar_t>(codePoint);
}

Diet parseDiet(std::string_view text, int lineNumber) {
    Diet diet;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t comma = text.find(',', start);
        const std::size_t partEnd = comma == std::string_view::npos ? text.size() : comma;
        const std::string_view part =
            rightTrimmed(leftTrimmed(text.substr(start, partEnd - start)));
        if (!part.empty()) {
            const auto category = parseOrganismCategory(part);
            if (!category) {
                throw SpeciesFileParseError(
                    lineNumber, "unknown diet category '" + std::string(part) + "'");
            }
            diet.allowCategory(*category);
        }
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
    return diet;
}

std::unique_ptr<Species> parseSpeciesLine(const std::string& line, int lineNumber) {
    std::unordered_map<std::string, std::string> fields;
    std::size_t start = 0;
    while (start < line.size()) {
        const std::size_t semicolon = line.find(';', start);
        const std::size_t tokenEnd =
            semicolon == std::string::npos ? line.size() : semicolon;
        const std::string_view token =
            leftTrimmed(std::string_view(line).substr(start, tokenEnd - start));
        if (!token.empty()) {
            const std::size_t equals = token.find('=');
            if (equals == std::string_view::npos) {
                throw SpeciesFileParseError(lineNumber, "expected key=value but found '" +
                                                            std::string(token) + "'");
            }
            const std::string key(rightTrimmed(token.substr(0, equals)));
            fields[key] = std::string(token.substr(equals + 1));
        }
        if (semicolon == std::string::npos) {
            break;
        }
        start = semicolon + 1;
    }

    const auto requireField = [&](const char* key) -> const std::string& {
        const auto found = fields.find(key);
        if (found == fields.end()) {
            throw SpeciesFileParseError(lineNumber,
                                        "missing field '" + std::string(key) + "'");
        }
        return found->second;
    };
    const auto requireDouble = [&](const char* key) -> double {
        const std::string& raw = requireField(key);
        const auto value = parseDouble(raw);
        if (!value) {
            throw SpeciesFileParseError(
                lineNumber, "field '" + std::string(key) + "' is not a number: " + raw);
        }
        return *value;
    };
    const auto requireInt = [&](const char* key) -> int {
        const std::string& raw = requireField(key);
        const auto value = parseInt(raw);
        if (!value) {
            throw SpeciesFileParseError(
                lineNumber, "field '" + std::string(key) + "' is not an integer: " + raw);
        }
        return *value;
    };

    auto species = std::make_unique<Species>();
    species->speciesId = requireField("id");
    species->displayName = requireField("name");

    const std::string& categoryText = requireField("category");
    const auto category = parseOrganismCategory(categoryText);
    if (!category) {
        throw SpeciesFileParseError(lineNumber, "unknown category '" + categoryText + "'");
    }
    species->category = *category;

    const std::string& glyphText = requireField("glyph");
    if (glyphText.empty()) {
        throw SpeciesFileParseError(lineNumber, "empty glyph");
    }
    species->glyph = decodeFirstCodePoint(glyphText);

    const std::string& colorText = requireField("color");
    const auto color = parseColor(colorText);
    if (!color) {
        throw SpeciesFileParseError(lineNumber, "unknown color '" + colorText + "'");
    }
    species->glyphColor = *color;

    species->ecoCreditCostToPlace = requireInt("cost");

    species->traits.idealTemperatureCelsius = requireDouble("idealTemp");
    species->traits.temperatureToleranceRange = requireDouble("tempTol");
    species->traits.idealHumidityPercent = requireDouble("idealHumidity");
    species->traits.humidityToleranceRange = requireDouble("humidityTol");
    species->traits.energyGainedPerFeeding = requireDouble("energyGain");
    species->traits.energyConsumedPerTick = requireDouble("energyPerTick");
    species->traits.energyRequiredToReproduce = requireDouble("energyToReproduce");
    species->traits.reproductionEnergyCost = requireDouble("reproCost");
    species->traits.reproductionCooldownTicks = requireInt("reproCooldown");
    species->traits.maximumAgeInTicks = requireInt("maxAge");
    species->traits.movementRangeInTiles = requireInt("moveRange");
    species->traits.seedDispersalProbabilityPerTick = requireDouble("seedProb");

    species->diet = parseDiet(requireField("diet"), lineNumber);
    return species;
}

}

SpeciesFileParseError::SpeciesFileParseError(int lineNumber, const std::string& message)
    : std::runtime_error(formatParseError(lineNumber, message)),
      offendingLineNumber(lineNumber) {}

void SpeciesRegistry::loadFromFile(const std::filesystem::path& speciesFilePath) {
    std::ifstream input(speciesFilePath);
    if (!input.is_open()) {
        throw SpeciesFileParseError(0, "cannot open " + speciesFilePath.string());
    }

    ownedSpecies.clear();
    speciesById.clear();

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::string_view content = leftTrimmed(line);
        if (content.empty() || content.front() == '#') {
            continue;
        }
        std::unique_ptr<Species> species = parseSpeciesLine(line, lineNumber);
        if (speciesById.contains(species->speciesId)) {
            throw SpeciesFileParseError(
                lineNumber, "duplicate species id '" + species->speciesId + "'");
        }
        speciesById.emplace(species->speciesId, species.get());
        ownedSpecies.push_back(std::move(species));
    }

    if (ownedSpecies.empty()) {
        throw SpeciesFileParseError(0, "no species defined in " + speciesFilePath.string());
    }
}

const Species* SpeciesRegistry::findSpeciesById(std::string_view speciesId) const {
    const auto found = speciesById.find(std::string(speciesId));
    return found == speciesById.end() ? nullptr : found->second;
}

std::vector<const Species*> SpeciesRegistry::allSpecies() const {
    std::vector<const Species*> result;
    result.reserve(ownedSpecies.size());
    for (const auto& species : ownedSpecies) {
        result.push_back(species.get());
    }
    return result;
}

}
