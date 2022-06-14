#include "winemaker.hpp"

#include <cmath>

#include "logger.hpp"
#include "tags.hpp"

#define format(fmt) "[{:0>10}] WINEMAKER #{} " fmt, __timestamp, __rank

namespace nouveaux {
    Winemaker::Winemaker(uint64_t safehouse_count, uint32_t rank, uint64_t students_start_id, uint64_t students_count, uint64_t winemakers_start_id, uint64_t winemakers_count, uint32_t min_wine_volume, uint32_t max_wine_volume)
      : __rng(std::random_device()()),
        __dist(min_wine_volume, max_wine_volume),
        __timestamp(0),
        __priority(0),
        __safehouse(rank % safehouse_count),
        __ack_counter(0),
        __pending_acks({}),
        __students_start_id(students_start_id),
        __students_count(students_count),
        __winemakers_start_id(winemakers_start_id),
        __winemakers_count(winemakers_count),
        __rank(rank) {}

    auto Winemaker::run() -> void {
        trace(format("STARTING."));
        // Run infinitely
        while (true) {
            info(format("sending aquire request for safehouse #{}"), __safehouse);
            send_req();
            __ack_counter = 0;

            while (__ack_counter < __winemakers_count - 1) {
                auto message = Message::receive_from(ANY_SOURCE);
                __timestamp = std::max(__timestamp, message.timestamp);
                if (message.type == Message::Type::WINEMAKER_ACKNOWLEDGE) {
                    debug(format("received WINEMAKER ACKNOWLEDGE {{ timestamp: {}, sender: {} }}"), message.timestamp, message.sender);
                    ++__ack_counter;
                } else if (message.type == Message::Type::WINEMAKER_REQUEST) {
                    debug(format("received WINEMAKER REQUEST {{ timestamp: {}, sender: {}, safehouse: {} }}"), message.timestamp, message.sender, message.payload.safehouse_index, message.payload.wine_volume);
                    if (message.timestamp < __priority || message.payload.safehouse_index != __safehouse) {
                        send_ack(message.sender);
                    } else if (message.timestamp == __priority && message.sender < __rank) {
                        send_ack(message.sender);
                    } else if (message.timestamp > __priority || (message.timestamp == __priority && message.sender > __rank)) {
                        __pending_acks.emplace_back(message);
                        ++__ack_counter;
                    }
                }
                trace(format("ACK COUNTER: {}"), __ack_counter);
            }

            auto volume = __dist(__rng);
            send_broadcast(volume);

            while (true) {
                auto message = Message::receive_from(ANY_SOURCE);
                __timestamp = std::max(__timestamp, message.timestamp);
                if (message.type == Message::Type::STUDENT_BROADCAST && message.payload.safehouse_index == __safehouse) {
                    debug(format("received STUDENT BROADCAST {{ timestamp: {}, sender: {}, safehouse: {} }}"), message.timestamp, message.sender, message.payload.safehouse_index);
                    for (auto&& m : __pending_acks) {
                        send_ack(m.sender);
                    }
                    __pending_acks.clear();
                    break;
                } else if (message.type == Message::Type::WINEMAKER_REQUEST) {
                    debug(format("received WINEMAKER REQUEST {{ timestamp: {}, sender: {}, safehouse: {} }}"), message.timestamp, message.sender, message.payload.safehouse_index, message.payload.wine_volume);
                    if (message.payload.safehouse_index != __safehouse) {
                        send_ack(message.sender);
                    } else {
                        __pending_acks.emplace_back(message);
                    }
                }
            }
        }
    }

    auto Winemaker::send_req() -> void {
        // Send request message for all winemakers, except itself.
        __priority = ++__timestamp;
        Message request {
            /* .type = */ Message::Type::WINEMAKER_REQUEST,
            /* .sender = */ __rank,
            /* .timestamp = */ __timestamp,
            /* .payload = */ Message::Payload {
              /* .safehouse_index = */ __safehouse,
              /* .wine_volume = */ 0,
              /* .last_timestamp = */ 0,
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
            /* .type = */ Message::Type::WINEMAKER_ACKNOWLEDGE,
            /* .sender = */ __rank,
            /* .timestamp = */ __timestamp,
            /* .payload = */ Message::Payload {
              /* .safehouse_index = */ 0,
              /* .wine_volume = */ 0,
              /* .last_timestamp = */ 0,
            },
        };

        ack.send_to(receiver);
    }

    auto Winemaker::send_broadcast(uint32_t volume) -> void {
        // Send broadcast message to all students.
        info(format("stores {} wine units in {}."), volume, __safehouse);

        ++__timestamp;
        Message broadcast {
            /* .type = */ Message::Type::WINEMAKER_BROADCAST,
            /* .sender = */ __rank,
            /* .timestamp = */ __timestamp,
            /* .payload = */ Message::Payload {
              /* .safehouse_index = */ __safehouse,
              /* .wine_volume = */ volume,
              /* .last_timestamp = */ 0,
            },
        };

        for (auto receiver = __students_start_id; receiver < __students_start_id + __students_count; ++receiver) {
            broadcast.send_to(receiver);
        }
    }
}
