#include "winemaker.hpp"

namespace nouveaux
{
    template <std::size_t safe_house_count>
    Winemaker<safe_house_count>::Winemaker(uint32_t min_wine_volume, uint32_t max_wine_volume)
    {
    }

    template <std::size_t safe_house_count>
    auto Winemaker<safe_house_count>::produce() -> void
    {
    }

    template <std::size_t safe_house_count>
    auto Winemaker<safe_house_count>::acquire_safe_place() -> std::size_t
    {
        return 0;
    }

    template <std::size_t safe_house_count>
    auto Winemaker<safe_house_count>::broadcast(uint32_t volume, std::size_t safe_house) -> void
    {
    }
} // namespace nouveau
