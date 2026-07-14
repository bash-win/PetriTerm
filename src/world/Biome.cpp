#include "petriterm/world/Biome.hpp"

#include <array>
#include <cstddef>

namespace petriterm::world {

using engine::TerminalColor;

const BiomeDescriptor& describeBiome(BiomeType biome) {
    static const std::array<BiomeDescriptor, 5> biomeDescriptors{{
        {BiomeType::TropicalJungle, "Tropical Jungle", L'^', TerminalColor::Green, 28.0,
         85.0, 0.9},
        {BiomeType::Desert, "Desert", L'∙', TerminalColor::Yellow, 35.0, 15.0, 0.2},
        {BiomeType::Grassland, "Grassland", L'"', TerminalColor::Green, 20.0, 50.0, 0.7},
        {BiomeType::Wetland, "Wetland", L'≈', TerminalColor::Cyan, 18.0, 90.0, 0.8},
        {BiomeType::Tundra, "Tundra", L'*', TerminalColor::White, -5.0, 40.0, 0.3},
    }};
    return biomeDescriptors[static_cast<std::size_t>(biome)];
}

}
