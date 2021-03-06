#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "message.hpp"

namespace nouveaux {
    class Student {
#if defined(NOUVEAUX_DEBUG)
      public:
#endif
        // Random number generator for generating wine demand.
        std::mt19937 __rng;
        // Random number distribution for limiting possible demand.
        // In theory this could be **any** distribution, uniform was just arbitrarily chosen.
        std::uniform_int_distribution<> __dist;
        // Current wine demand.
        //
        // MUTABILITY: Should change only:
        //     1) When zeroed -> generating.
        //     2) When wine is consumed after safehouse acquisition.
        uint32_t __demand;
        // State of available wine supplies in every safehouse.
        //
        // MUTABILITY: Should change only:
        //     1) When received WINEMAKER_BROADCAST message.
        //     2) When received STUDENT_ACQUISITION_REQ message.
        //     3) When student acquire safehouse.
        //
        // SAFETY: Every modification on this vector should be checked for overflow,
        // as it's highly possible to try to insert negative value here.
        // Integer overflow in C++ is Undefined Behavior.
        std::vector<uint64_t> __safehouses;
        // Lamport logical clock for message timestamps.
        //
        // MUTABILITY: Should change on internal events and when message is sent or received.
        uint64_t __timestamp;
        // Lamport clock state for last sent REQ message.
        //
        // MUTABILITY: Should change only when REQ message is being sent.
        uint64_t __priority;
        // Currently chosen safehouse index.
        //
        // MUTABILITY: Should change only when new safehouse acquisition is started.
        uint64_t __safehouse;
        // Number of received ACKs when acquiring safehouse.
        //
        // MUTABILITY: Should change only:
        //     1) When received STUDENT_ACQUISITION_ACK message with apropriate safehouse index.
        //     2) When received STUDENT_ACQUISITION_REQ message with apropriate safehouse index and higher timestamp than current priority.
        uint64_t __ack_counter;
        // List of processes waiting for ACK with corresponding safehouse index.
        //
        // MUTABILITY: Should change only:
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
        Student(uint64_t safehouse_count, uint32_t rank, uint64_t students_start_id, uint64_t students_count, uint64_t winemakers_start_id, uint64_t winemakers_count, uint32_t min_wine_volume, uint32_t max_wine_volume);
        auto run() -> void;

      private:
        auto send_req() -> void;
        auto send_ack(Message request) -> void;
        auto send_broadcast(uint64_t safehouse) -> void;
    };
}
