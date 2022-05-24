#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "message.hpp"

namespace nouveaux {
    class Student {
#if defined(__WINEMAKER_DEBUG__)
      public:
#endif
        // Random number generator for generating wine demand.
        std::mt19937 __rng;
        // Random number distribution for limiting possible demand.
        // In theory this could be **any** distribution, uniform was just arbitrarily chosen.
        std::uniform_int_distribution<> __dist;
        // Current wine demand.
        //
        // MUTABILITY: Should change only in two places:
        //     1) When generating demand in demand() method.
        //     2) When wine is consumed in consume() method.
        uint32_t __demand;
        // State of available wine supplies in every safehouse.
        //
        // MUTABILITY: Should change only in three places (all in handle_message() method):
        //     1) When received WINEMAKER_BROADCAST message.
        //     2) When received STUDENT_ACQUISITION_REQ message.
        //     3) When student acquire safehouse and get supplies.
        //
        // SAFETY: Every modification on this vector should be checked for overflow,
        // as it's highly possible to try to insert negative value here.
        // Integer overflow in C++ is Undefined Behavior.
        std::vector<uint64_t> __safehouses;
        // Lamport logical clock for message timestamps.
        //
        // MUTABILITY: Should change every time message is sent or received.
        uint64_t __timestamp;
        // Lamport clock state for last sent REQ message.
        //
        // MUTABILITY: Should change only when REQ message is being sent.
        uint64_t __priority;
        // Currently chosen safehouse index.
        //
        // MUTABILITY: Should change only in acquire_safe_place() method.
        uint64_t __safehouse;
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
        Student(uint64_t safehouse_count, uint32_t rank, uint64_t students_start_id, uint64_t students_count, uint64_t winemakers_start_id, uint64_t winemakers_count, uint32_t min_wine_volume, uint32_t max_wine_volume);
        auto run() -> void;

      private:
        auto demand() -> void;
        auto consume() -> void;
        auto satisfy_demand() -> void;
        auto handle_message(Message message) -> void;
        auto listen_for_messages() -> void;
        auto acquire_safe_place() -> void;

        auto acquisition_cleanup() -> void;
        auto send_req() -> void;
        auto send_ack(Message request) -> void;
        auto send_broadcast(uint64_t safehouse) -> void;
    };
}
