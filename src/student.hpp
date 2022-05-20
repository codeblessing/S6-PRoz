#pragma once

#include <random>
#include <cstdint>
#include <vector>
#include <array>

#include "message.hpp"

namespace nouveaux
{
    class Student
    {
#ifdef __WINEMAKER_TEST__
    public:
#endif
        // `__rng` and `__dist` are for "consuming" wine.
        std::mt19937 __rng;
        std::uniform_int_distribution<> __dist;
        // Remaining units to consume.
        uint32_t __remaining_wine_demand;
        // State of all safehouses.
        std::vector<uint64_t> __safehouses; 
        // Currently chosen safehouse index.
        uint64_t __safehouse;
        // Range of students' ids.
        std::array<uint64_t, 2> __students;
        // Number of students.
        uint32_t __students_count;
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
        Student(uint64_t safehouse_count, int32_t rank, int32_t system_size, std::array<uint64_t, 2> &&students, uint32_t min_wine_volume, uint32_t max_wine_volume);
        auto run() -> void;

    private:
        auto consume() -> void;
        auto handle_message(Message message) -> void;
        auto listen_for_messages() -> void;
        auto acquire_safe_place() -> void;
        auto broadcast(uint64_t safe_house) -> void;
    };
}
