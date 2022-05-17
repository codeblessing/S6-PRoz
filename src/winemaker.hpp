#pragma once

#include <random>
#include <cstdint>
#include <vector>
#include <array>
#include <mutex>

#include "message.hpp"

namespace nouveaux
{
    class Winemaker {
        
    #ifdef __WINEMAKER_TEST__

    public:

    #endif
        // `__rng` and `__dist` are for "producing" wine.
        std::mt19937 __rng;
        std::uniform_int_distribution<> __dist;
        // For simplicity & scalability every winemaker tries to acquire every time the same safehouse.
        // By convention this safehouse will be <winemakers id> mod <safehouse count>.
        uint64_t __safehouse;
        // Range of winemakers' ids.
        std::array<uint32_t, 2> __winemakers;
        uint32_t __winemakers_count;
        // Lamport logical clock for message timestamps.
        uint64_t __lamport;
        // Lamport clock state for last sent REQ message.
        uint64_t __last_req_lamport;
        // Process's own id.
        int32_t __rank;
        // Number of working processes.
        int32_t __system_size;
        // In progress safehouse acquisition indicator.
        bool __acquiring_safehouse;
        // We're holding the safehouse;
        bool __safehouse_acquired;
        // Number of received ACKs when acquiring safehouse.
        uint64_t __ack_counter;
        // List of processes waiting for ACK.
        std::vector<uint64_t> __pending_ack;

    public:
        Winemaker(std::size_t safe_house_count, std::array<uint32_t, 2> winemakers_index_range, uint32_t min_wine_volume, uint32_t max_wine_volume);
        auto run() -> void;
    private:
        auto produce() -> void;
        auto handle_message(Message message) -> void;
        auto listen_for_messages() -> void;
        auto acquire_safe_place() -> void; 
        auto broadcast(uint32_t volume, uint64_t safe_house) -> void;
    };
}
