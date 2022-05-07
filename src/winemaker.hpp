#pragma once

#include <random>
#include <cstdint>
#include <array>

namespace nouveaux
{
    template<std::size_t safe_house_count>
    class Winemaker {
        std::mt19937 __rng;
        std::uniform_int_distribution<> __dist;
        std::array<uint32_t, safe_house_count> __safe_houses;

    public:
        Winemaker(uint32_t min_wine_volume, uint32_t max_wine_volume);
        auto produce() -> void;

    private:
        auto acquire_safe_place() -> std::size_t; 
        auto broadcast(uint32_t volume, std::size_t safe_house) -> void;
    };
}
