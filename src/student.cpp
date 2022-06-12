#include "student.hpp"

#include <algorithm>

#include <fmt/format.h>

#include "logger.hpp"
#include "message.hpp"
#include "tags.hpp"

// // !!! DEBUG !!!
// #include <chrono>
// #include <thread>
// using namespace std::chrono_literals;
// // !!! /DEBUG !!!

#define format(fmt) "[{:0>10}] STUDENT #{} " fmt, __timestamp, __rank

namespace nouveaux {
    Student::Student(uint64_t safehouse_count, uint32_t rank, uint64_t students_start_id, uint64_t students_count, uint64_t winemakers_start_id, uint64_t winemakers_count, uint32_t min_wine_volume, uint32_t max_wine_volume)
      : __rng(std::random_device()()),
        __dist(min_wine_volume, max_wine_volume),
        __demand(0),
        __safehouses({}),
        __timestamp(0),
        __priority(0),
        __safehouse(0),
        __ack_counter(0),
        __acquiring_safehouse(false),
        __pending_acks({}),
        __students_start_id(students_start_id),
        __students_count(students_count),
        __winemakers_start_id(winemakers_start_id),
        __winemakers_count(winemakers_count),
        __rank(rank) {
        __safehouses.reserve(safehouse_count);
        for (uint64_t i = 0; i < safehouse_count; ++i)
            __safehouses.push_back(0);
    }

    auto Student::run() -> void {
        info(format("STARTING."));
        listen_for_messages();
    }

    auto Student::demand() -> void {
        if (__demand == 0) {
            ++__timestamp;
            __demand = __dist(__rng);
        }
        debug(format("demand: {} wine units."), __demand);
    }

    auto Student::consume() -> void {
        __ack_counter = 0;
        __acquiring_safehouse = false;
        // Internal event happened (safehouse acquired) -> increment clock by 1.
        ++__timestamp;

        trace(format("safehouse acquire state {{ remaining demand: {}, safehouse #{} supplies: {} }}"), __demand, __safehouse, __safehouses[__safehouse]);
        // Wine units we can take from safehouse.
        const uint64_t volume = std::min(__safehouses[__safehouse], static_cast<uint64_t>(__demand));
        __safehouses[__safehouse] -= volume;
        __demand -= volume;

        if (__safehouses[__safehouse] == 0) {
            // We emptied out safehouse -> send STUDENT BROADCAST message to winemakers.
            send_broadcast(__safehouse);
        }

        trace(format("safehouse release state {{ remaining demand: {}, safehouse #{} supplies: {} }}"), __demand, __safehouse, __safehouses[__safehouse]);

        // Send ACK for pending students and update supplies appropriately.
        for (auto message : __pending_acks) {
            send_ack(message);
            const auto volume = std::min(__safehouses[message.payload.safehouse_index], message.payload.wine_volume);
            __safehouses[message.payload.safehouse_index] -= volume;
        }
        // Clear ACK queue.
        __pending_acks.clear();

        // Go for more wine...
        satisfy_demand();
    }

    auto Student::satisfy_demand() -> void {
        demand();
        acquire_safe_place();
    }

    auto Student::handle_message(Message message) -> void {
        __timestamp = std::max(__timestamp, message.timestamp) + 1;

        switch (message.type) {
            case Message::Type::WINEMAKER_BROADCAST: {
                handle_winemaker_broadcast(std::move(message));
                break;
            }
            case Message::Type::STUDENT_REQUEST: {
                handle_student_request(std::move(message));
                trace(format("ACK COUNTER: {}"), __ack_counter);
                break;
            }
            case Message::Type::STUDENT_ACKNOWLEDGE: {
                handle_student_acknowledge(std::move(message));
                trace(format("ACK COUNTER: {}"), __ack_counter);
            }
            default:
                break;
        }

        if (__ack_counter == __students_count - 1) {
            // All required ACKs were gathered.
            consume();
        }
    }

    auto Student::listen_for_messages() -> void {
        while (true) {
            // std::this_thread::sleep_for(500ms);
            if (__students_count < 2 && __acquiring_safehouse) {
                debug("Student hit the SINGLE STUDENT point.\n");
                consume();
            } else {
                auto message = Message::receive_from(MPI_ANY_SOURCE);
                handle_message(message);
            }
        }
    }

    auto Student::acquire_safe_place() -> void {
        if (__acquiring_safehouse) {
            error(format("[STU] ERROR: New acquisition request during active acquisition."));
            return;
        }

        // TODO: Implement more sophisticated algorithm, that chooses best fit.
        // Currently used: simplest greedy algorithm that takes first non-empty safehouse.
        auto index = 0;
        for (auto&& safehouse : __safehouses) {
            if (safehouse > 0) {
                __acquiring_safehouse = true;
                __safehouse = index;
                break;
            }
            ++index;
        }

        if (__acquiring_safehouse) {
            debug(format("trying to acquire safehouse #{}"), __safehouse);
            send_req();
        }
    }

    auto Student::handle_winemaker_broadcast(Message&& message) -> void {
        debug(format("received WINEMAKER BROADCAST {{ timestamp: {}, sender: {}, safehouse: {}, volume: {} }}"), message.timestamp, message.sender, message.payload.safehouse_index, message.payload.wine_volume);
        __safehouses[message.payload.safehouse_index] = message.payload.wine_volume;
        // If we're not currently acquiring safehouse (so either we had no demand or all safehouses were empty)
        if (!__acquiring_safehouse) {
            satisfy_demand();
        }
    }

    auto Student::handle_student_request(Message&& message) -> void {
        debug(format("received STUDENT REQUEST {{ timestamp: {}, sender: {}, safehouse: {}, volume: {} }}"), message.timestamp, message.sender, message.payload.safehouse_index, message.payload.wine_volume);
        const uint64_t volume = std::min(__safehouses[message.payload.safehouse_index], message.payload.wine_volume);

        if (message.payload.safehouse_index == __safehouse && __acquiring_safehouse) {
            if ((message.timestamp > __priority) || ((message.timestamp == __priority) && (message.sender > __rank))) {
                ++__ack_counter;
                __pending_acks.emplace_back(message);
            } else {
                __safehouses[message.payload.safehouse_index] -= volume;
                if (__safehouses[message.payload.safehouse_index] == 0) {
                    // Invalidate current acquisition and try another one.
                    acquisition_cleanup();
                    satisfy_demand();
                }
            }
        } else {
            __safehouses[message.payload.safehouse_index] -= volume;
            send_ack(message);
        }
    }

    auto Student::handle_student_acknowledge(Message&& message) -> void {
        debug(format("received STUDENT ACKNOWLEDGE {{ timestamp: {}, sender: {}, safehouse: {}, request timestamp: {} }}"), message.timestamp, message.sender, message.payload.safehouse_index, message.payload.last_timestamp);
        if (__acquiring_safehouse && message.payload.last_timestamp == __priority) {
            ++__ack_counter;
        }
    }

    auto Student::acquisition_cleanup() -> void {
        __ack_counter = 0;
        __acquiring_safehouse = false;

        for (auto&& message : __pending_acks) {
            send_ack(message);
            const auto volume = std::min(__safehouses[message.payload.safehouse_index], message.payload.wine_volume);
            __safehouses[message.payload.safehouse_index] -= volume;
        }

        __pending_acks.clear();
    }

    auto Student::send_req() -> void {
        __priority = ++__timestamp;
        Message request {
            Message::Type::STUDENT_REQUEST,
            __rank,
            __timestamp,
            Message::Payload {
              __safehouse,
              __demand,
              0,
            },
        };

        for (auto receiver = __students_start_id; receiver < __students_start_id + __students_count; ++receiver) {
            if (receiver != __rank) {
                request.send_to(receiver);
            }
        }
    }

    auto Student::send_ack(Message request) -> void {
        ++__timestamp;
        Message ack {
            /* .type = */ Message::Type::STUDENT_ACKNOWLEDGE,
            /* .sender = */ __rank,
            /* .timestamp = */ __timestamp,
            /* .payload = */ Message::Payload {
              /* .safehouse_index = */ request.payload.safehouse_index,
              /* .wine_volume = */ 0,
              /* .last_timestamp = */ request.timestamp,
            }
        };

        ack.send_to(request.sender);
    }

    auto Student::send_broadcast(uint64_t safehouse) -> void {
        debug(format("emptied out safehouse #{}."), __safehouse);

        ++__timestamp;
        Message broadcast {
            /* .type = */ Message::Type::STUDENT_BROADCAST,
            /* .sender = */ __rank,
            /* .timestamp = */ __timestamp,
            /* .payload = */ Message::Payload {
              /* .safehouse_index = */ safehouse,
              /* .wine_volume = */ 0,
              /* .last_timestamp = */ 0,
            }
        };

        for (auto receiver = __winemakers_start_id; receiver < __winemakers_start_id + __winemakers_count; ++receiver) {
            broadcast.send_to(receiver);
        }
    }
}