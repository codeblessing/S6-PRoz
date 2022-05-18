#pragma once

#include <random>
#include <cstdint>
#include <vector>
#include <array>
#include <mutex>

#include "message.hpp"

namespace nouveaux
{
    class Winemaker
    {
        /// This class is just a little bloated workaround for nonexisting in C++ lateinit consts.
        class Builder
        {
            int32_t rank;
            int32_t system_size;
            uint32_t min_wine_volume;
            uint32_t max_wine_volume;
            uint64_t safehouse;
            std::array<uint64_t, 2> winemakers;

        public:
            Builder(uint64_t safehouse_count, std::array<uint64_t, 2> &&winemakers);
            /// Changes possible wine volume boundaries.
            /// Swaps values if min_volume > max_volume.
            auto wine_volume(uint32_t min_volume, uint32_t max_volume) -> Builder;
            auto build() -> Winemaker;
        };

#ifdef __WINEMAKER_TEST__

    public:
#endif
        // `__rng` and `__dist` are for "producing" wine.
        std::mt19937 __rng;
        std::uniform_int_distribution<> __dist;
        // For simplicity & scalability every winemaker tries to acquire every time the same safehouse.
        // By convention this safehouse will be <winemakers id> mod <safehouse count>.
        const uint64_t __safehouse;
        // Range of winemakers' ids.
        std::array<uint64_t, 2> __winemakers;
        // Number of winemakers.
        uint32_t __winemakers_count;
        // Process's own id.
        int32_t __rank;
        // Number of working processes.
        int32_t __system_size;
        // Lamport logical clock for message timestamps.
        uint64_t __timestamp;
        // Lamport clock state for last sent REQ message.
        uint64_t __current_priority;
        // Number of received ACKs when acquiring safehouse.
        uint64_t __ack_counter;
        // In progress safehouse acquisition indicator.
        bool __acquiring_safehouse;
        // We're holding the safehouse;
        bool __safehouse_acquired;
        // List of processes waiting for ACK.
        std::vector<uint64_t> __pending_ack;

    public:
        /// Creates Winemaker class builder.
        ///
        /// `safehouse_count` - number of safehouses.
        /// `winemakers` - first and last index (rank) of winemakers group. Winemakers' indices are expected to create continuous range.
        static auto builder(uint64_t safehouse_count, std::array<uint64_t, 2> winemakers) -> Builder;
        auto run() -> void;

    private:
        Winemaker(int32_t rank, int32_t system_size, uint64_t safehouse, std::array<uint64_t, 2> winemakers, uint32_t min_wine_volume, uint32_t max_wine_volume);
        auto produce() -> void;
        auto handle_message(Message message) -> void;
        auto listen_for_messages() -> void;
        auto acquire_safe_place() -> void;
        auto broadcast(uint32_t volume, uint64_t safe_house) -> void;
    };

}
