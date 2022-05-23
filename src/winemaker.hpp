#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "message.hpp"

namespace nouveaux {
    class Winemaker {
#if defined(__WINEMAKER_DEBUG__)
      public:
#endif
        // Random number generator for generating wine supply.
        std::mt19937 __rng;
        // Random number distribution for limiting possible supply.
        // In theory this could be **any** distribution, uniform was just arbitrarily chosen.
        std::uniform_int_distribution<> __dist;
        // Lamport logical clock for message timestamps.
        //
        // MUTABILITY: Should change every time message is sent or received.
        uint64_t __timestamp;
        // Lamport clock state for last sent REQ message.
        //
        // MUTABILITY: Should change only when REQ message is being sent.
        uint64_t __priority;
        // For simplicity & scalability every winemaker tries to acquire every time the same safehouse.
        // By convention this safehouse will be <winemakers id> mod <safehouse count>.
        const uint64_t __safehouse;
        // Number of received ACKs when acquiring safehouse.
        //
        // MUTABILITY: Should change only in two places (both in handle_message() method):
        //     1) When received STUDENT_ACQUISITION_ACK message with apropriate safehouse index.
        //     2) When received STUDENT_ACQUISITION_REQ message with apropriate safehouse index and higher timestamp than current priority.
        uint64_t __ack_counter;
        // In progress safehouse acquisition indicator.
        //
        // MUTABILITY: Should change only in two places:
        //     1) When starting safeplace acquisition in acquire_safe_place() method.
        //     2) When student acquire safehouse in handle_message() method.
        bool __acquiring_safehouse;
        // Safehouse ownership indicator.
        //
        // MUTABILITY: Should change only in two places:
        //     1) When winemaker acquires safehouse.
        //     2) When received STUDENT_BROADCAST message with appropriate safehouse index.
        bool __acquired_safehouse;
        // List of processes waiting for ACK with corresponding safehouse index.
        //
        // MUTABILITY: Should change only in two places:
        //     1) When received STUDENT_ACQUISITION_REQ message with apropriate safehouse index and higher timestamp than current priority.
        //     2) When student acquired safehouse and modified apropriate values.
        std::vector<Message> __pending_acks;
        // Lower (inclusive) bound of students' ids.
        const uint64_t __students_start_id;
        // Number of students.
        const uint64_t __students_count;
        // Lower (inclusive) bound of winemakers' ids.
        const uint64_t __winemakers_start_id;
        // Number of winemakers.
        const uint64_t __winemakers_count;
        // Process's own id.
        const uint32_t __rank;

      public:
        Winemaker(uint64_t safehouse_count, uint32_t rank, uint64_t students_start_id, uint64_t students_count, uint64_t winemakers_start_id, uint64_t winemakers_count, uint32_t min_wine_volume, uint32_t max_wine_volume);
        auto run() -> void;

      private:
        auto produce() -> void;
        auto handle_message(Message message) -> void;
        auto listen_for_messages() -> void;
        auto acquire_safe_place() -> void;

        auto send_req() -> void;
        auto send_ack(uint64_t receiver) -> void;
        auto send_broadcast(uint32_t volume) -> void;
    };

}
