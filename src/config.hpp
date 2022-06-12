#pragma once

#include <cstdint>
#include <string>

#include <toml.hpp>

namespace nouveaux {

    struct Config {
        uint64_t safehouse_count;
        uint64_t winemaker_count;
        uint64_t student_count;
        uint32_t min_wine_volume;
        uint32_t max_wine_volume;

        static auto parse(const std::string& filename) -> Config;
    };

    inline auto Config::parse(const std::string& filename) -> Config {
        auto src = toml::parse(filename);

        auto safehouse_count = toml::find_or<uint64_t>(src, "safehouse_count", 1);
        auto winemaker_count = toml::find_or<uint64_t>(src, "winemaker_count", 1);
        auto student_count = toml::find_or<uint64_t>(src, "student_count", 1);
        auto min_wine_volume = toml::find_or<uint32_t>(src, "min_wine_volume", 1);
        auto max_wine_volume = toml::find_or<uint32_t>(src, "max_wine_volume", 150);

        return Config {
            safehouse_count,
            winemaker_count,
            student_count,
            min_wine_volume,
            max_wine_volume
        };
    }
}
