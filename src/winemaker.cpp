#include "winemaker.hpp"

#include <cmath>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <mpi/mpi.h>

#include "tags.hpp"

namespace nouveaux {
    Winemaker::Winemaker(uint64_t safehouse_count, uint32_t rank, uint64_t students_start_id, uint64_t students_count, uint64_t winemakers_start_id, uint64_t winemakers_count, uint32_t min_wine_volume, uint32_t max_wine_volume)
      : __rng(std::random_device()()),
        __dist(min_wine_volume, max_wine_volume),
        __timestamp(0),
        __priority(0),
        __safehouse(rank % safehouse_count),
        __ack_counter(0),
        __acquiring_safehouse(false),
        __acquired_safehouse(false),
        __pending_acks({}),
        __students_start_id(students_start_id),
        __students_count(students_count),
        __winemakers_start_id(winemakers_start_id),
        __winemakers_count(winemakers_count),
        __rank(rank) {}

    auto Winemaker::run() -> void {
        acquire_safe_place();
        listen_for_messages();
    }

    auto Winemaker::produce() -> void {
        fmt::print("Winemaker #{} acquired safehouse #{}\n", __rank, __safehouse);
        __acquiring_safehouse = false;
        __acquired_safehouse = true;

        auto volume = __dist(__rng);
        send_broadcast(volume);
    }

    auto Winemaker::listen_for_messages() -> void {
        while (true) {
            if (__winemakers_count < 2 && __acquiring_safehouse) {
                fmt::print("Winemaker hit the SINGLE WINEMAKER point.\n");
                produce();
            } else {
                auto message = Message::receive_from(MPI_ANY_SOURCE);
                handle_message(message);
            }
        }
    }

    auto Winemaker::handle_message(Message message) -> void {
        __timestamp = std::max(__timestamp, message.timestamp) + 1;

        switch (message.type) {
            case Message::Type::WINEMAKER_REQUEST: {
                // fmt::print("Winemaker #{} received REQUEST message from winemaker #{}.\nMessage details:\n\ttimestamp: {}\n\tsafehouse: {}\n", __rank, message.sender, message.timestamp, message.payload.safehouse_index);
                if (message.payload.safehouse_index == __safehouse) {
                    if ((message.timestamp > __priority) || ((message.timestamp == __priority) && (message.sender > __rank))) {
                        ++__ack_counter;
                        __pending_acks.emplace_back(message);
                    }
                } else {
                    send_ack(message.sender);
                }
                break;
            }
            case Message::Type::WINEMAKER_ACKNOWLEDGE: {
                ++__ack_counter;
                break;
            }
            case Message::Type::STUDENT_BROADCAST: {
                // fmt::print("Winemaker #{} received BROADCAST message from Student #{}.\n", __rank, message.sender);
                if (message.payload.safehouse_index == __safehouse) {
                    if (__acquired_safehouse) {
                        fmt::print("Winemaker #{} freed safehouse #{}.\n", __rank, __safehouse);
                        __acquired_safehouse = false;
                        for (auto message : __pending_acks) {
                            send_ack(message.sender);
                        }

                        __pending_acks.clear();
                    };

                    acquire_safe_place();
                }
                break;
            }

            default:
                break;
        }

        if (__ack_counter == __winemakers_count - 1) {
            produce();
        }
    }

    auto Winemaker::acquire_safe_place() -> void {
        if (__acquiring_safehouse || __acquired_safehouse) {
            fmt::print(stderr, "[MAK] ERROR: New acquisition request during active acquisition.\n");
            return;
        }

        fmt::print("Winemaker #{} wants to acquire safehouse #{}.\n", __rank, __safehouse);

        __acquiring_safehouse = true;
        __ack_counter = 0;

        if (__winemakers_count > 1) {
            send_req();
        }
    }

    auto Winemaker::send_req() -> void {
        __priority = ++__timestamp;
        Message request {
            .type = Message::Type::WINEMAKER_REQUEST,
            .sender = __rank,
            .timestamp = __timestamp,
            .payload = Message::Payload {
              .safehouse_index = __safehouse,
              .wine_volume = 0,
            },
        };

        for (auto receiver = __winemakers_start_id; receiver < __winemakers_start_id + __winemakers_count; ++receiver) {
            if (receiver != __rank) {
                request.send_to(receiver);
            }
        }
    }

    auto Winemaker::send_ack(uint64_t receiver) -> void {
        ++__timestamp;
        Message ack {
            .type = Message::Type::WINEMAKER_ACKNOWLEDGE,
            .sender = __rank,
            .timestamp = __timestamp,
            .payload = Message::Payload {
              .safehouse_index = 0,
              .wine_volume = 0,
            },
        };

        ack.send_to(receiver);
    }

    auto Winemaker::send_broadcast(uint32_t volume) -> void {
        fmt::print("Winemaker #{} stores {} wine units in {}.\n", __rank, volume, __safehouse);

        ++__timestamp;
        Message broadcast {
            .type = Message::Type::WINEMAKER_BROADCAST,
            .sender = __rank,
            .timestamp = __timestamp,
            .payload = Message::Payload {
              .safehouse_index = __safehouse,
              .wine_volume = volume,
            },
        };

        for (auto receiver = __students_start_id; receiver < __students_start_id + __students_count; ++receiver) {
            broadcast.send_to(receiver);
        }
    }
}
