#include "student.hpp"

#include <algorithm>

#include <fmt/format.h>
#include <mpi/mpi.h>

#include "tags.hpp"

// !!! DEBUG !!!
#include <chrono>
#include <thread>
using namespace std::chrono_literals;
// !!! /DEBUG !!!

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
        fmt::print("Student #{} is starting.\n", __rank);
        listen_for_messages();
    }

    auto Student::demand() -> void {
        if (__demand == 0)
            __demand = __dist(__rng);
        fmt::print("Student #{} needs {} wine units.\n", __rank, __demand);
    }

    auto Student::consume() -> void {
        // If we hit all ACKs we're holding safehouse.
        if (__ack_counter == __students_count - 1) {
            fmt::print("Student #{} acquired safehouse {}.\n", __rank, __safehouse);
            __acquiring_safehouse = false;
            __ack_counter = 0;

            if (__safehouses[__safehouse] >= __demand) {
                __safehouses[__safehouse] -= __demand;
                __demand = 0;
            } else {
                __demand -= __safehouses[__safehouse];
                __safehouses[__safehouse] = 0;
                send_broadcast(__safehouse);
            }
            fmt::print("Student #{} reports state:\n\tdemand: {}\n\tsafehouse #{}: {}\n", __rank, __demand, __safehouse, __safehouses[__safehouse]);
            fmt::print("Student #{} releases safehouse #{}.\n", __rank, __safehouse);

            send_pending_acks();

            satisfy_demand();
        }
    }

    auto Student::satisfy_demand() -> void {
        demand();
        acquire_safe_place();
    }

    auto Student::handle_message(Message message) -> void {
        __timestamp = std::max(__timestamp, message.timestamp) + 1;

        switch (message.type) {
            case Message::Type::WINEMAKER_BROADCAST: {
                fmt::print("Student #{} received BROADCAST from winemaker #{}.\n", __rank, message.sender);
                __safehouses[message.payload.safehouse_index] = message.payload.wine_volume;
                // If we're not currently acquiring safehouse (so either we had no demand or all safehouses were empty)
                if (!__acquiring_safehouse)
                    satisfy_demand();
                break;
            }
            // case Message::Type::STUDENT_REQUEST: {
            //     // If received request has lower priority (higher lamport clock) then we can treat this as ACK
            //     // but we need to remember to send ACK when we will free the safehouse.
            //     if (__acquiring_safehouse && message.payload.safehouse_index == __safehouse) {
            //         if (message.timestamp > __priority || (message.timestamp == __priority && message.sender > __rank)) {
            //             ++__ack_counter;
            //             __pending_acks.emplace_back(message);
            //         } else {
            //             __safehouses[message.payload.safehouse_index] -= message.payload.wine_volume;
            //             if (__safehouses[message.payload.safehouse_index] < 1) {
            //                 // This one has taken all wine from our safehouse, so we
            //                 // invalidate our current acquisition and request other safehouse.
            //                 acquisition_cleanup();
            //                 satisfy_demand();
            //             }
            //         }
            //     } else {
            //         send_pending_acks();
            //     }

            //     break;
            // }
            // case Message::Type::STUDENT_ACKNOWLEDGE: {
            //     if (__acquiring_safehouse && message.payload.safehouse_index == __safehouse)
            //         ++__ack_counter;
            // }
            default:
                fmt::print("Student #{} received OTHER message.\n", __rank);
                break;
        }

        consume();
    }

    auto Student::listen_for_messages() -> void {
        while (true) {
            std::this_thread::sleep_for(500ms);
            if (__students_count < 2 && __acquiring_safehouse) {
                fmt::print("Student hit the SINGLE STUDENT point.\n");
                consume();
            } else {
                auto message = Message::receive_from(MPI_ANY_SOURCE);
                handle_message(message);
            }
        }
    }

    auto Student::acquire_safe_place() -> void {
        if (__acquiring_safehouse) {
            fmt::print(stderr, "[STU] ERROR: New acquisition request during active acquisition.\n");
            return;
        }

        // TODO: Implement more sophisticated algorithm, that chooses best fit.
        // Currently used: simplest greedy algorithm that takes first non-empty safehouse.
        for (auto index = 0; auto&& safehouse : __safehouses) {
            if (safehouse > 0) {
                __acquiring_safehouse = true;
                __safehouse = index;
                break;
            }
        }

        if (__acquiring_safehouse) {
            fmt::print("Student #{} wants to acquire safehouse #{}\n", __rank, __safehouse);
            send_req();
        }
    }

    auto Student::acquisition_cleanup() -> void {
        __ack_counter = 0;
        __acquiring_safehouse = false;

        send_pending_acks();
    }

    auto Student::send_pending_acks() -> void {
        for (auto&& [_type, receiver, _timestamp, payload] : __pending_acks) {
            // Safehouse state must be updated if we haven't used all the wine from it.
            const auto volume_taken = std::min(__safehouses[payload.safehouse_index], payload.wine_volume);
            __safehouses[payload.safehouse_index] -= volume_taken;

            fmt::print("Student #{} sends ACK({}) to #{}.\n", __rank, payload.safehouse_index, receiver);
            send_ack(receiver, payload.safehouse_index);
        }

        __pending_acks.clear();
    }

    auto Student::send_req() -> void {
        __priority = ++__timestamp;
        Message request {
            .type = Message::Type::STUDENT_REQUEST,
            .sender = __rank,
            .timestamp = __timestamp,
            .payload = Message::Payload {
              .safehouse_index = __safehouse,
              .wine_volume = __demand,
            },
        };

        for (auto receiver = __students_start_id; receiver < __students_start_id + __students_count; ++receiver) {
            if (receiver != __rank) {
                request.send_to(receiver);
            }
        }
    }

    auto Student::send_ack(uint64_t receiver, uint64_t safehouse) -> void {
        ++__timestamp;
        Message ack {
            .type = Message::Type::STUDENT_ACKNOWLEDGE,
            .sender = __rank,
            .timestamp = __timestamp,
            .payload = Message::Payload {
              .safehouse_index = safehouse,
              .wine_volume = 0,
            }
        };

        ack.send_to(receiver);
    }

    auto Student::send_broadcast(uint64_t safehouse) -> void {
        fmt::print("Student #{} emptied out safehouse #{}.\n", __rank, __safehouse);

        ++__timestamp;
        Message broadcast {
            .type = Message::Type::STUDENT_BROADCAST,
            .sender = __rank,
            .timestamp = __timestamp,
            .payload = Message::Payload {
              .safehouse_index = safehouse,
              .wine_volume = 0,
            }
        };

        for (auto receiver = __winemakers_start_id; receiver < __winemakers_start_id + __winemakers_count; ++receiver) {
            broadcast.send_to(receiver);
        }
    }
}