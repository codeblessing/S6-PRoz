#include "student.hpp"

#include <algorithm>

#include "logger.hpp"
#include "message.hpp"
#include "tags.hpp"

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
        trace(format("STARTING."));
        bool skip_safehouse = false;
        // Run infinitely
        while (true) {
            __demand = __dist(__rng);
            debug(format("DEMAND: {}"), __demand);

            while (__demand != 0) {
                // Find non-empty safehouse
                __safehouse = std::numeric_limits<uint64_t>::max();
                auto index = 0;
                for (auto&& safehouse : __safehouses) {
                    if (safehouse > 0) {
                        __safehouse = index;
                        break;
                    }
                    ++index;
                }

                while (__safehouse == std::numeric_limits<uint64_t>::max()) {
                    trace(format("ALL SAFEHOUSES EMPTY."));
                    auto message = Message::receive_from(ANY_SOURCE);
                    __timestamp = std::max(__timestamp, message.timestamp) + 1;
                    if (message.type == Message::Type::WINEMAKER_BROADCAST) {
                        debug(format("received WINEMAKER BROADCAST {{ timestamp: {}, sender: {}, safehouse: {}, volume: {} }}"), message.timestamp, message.sender, message.payload.safehouse_index, message.payload.wine_volume);
                        __safehouses[message.payload.safehouse_index] = message.payload.wine_volume;
                        __safehouse = message.payload.safehouse_index;
                    } else if (message.type == Message::Type::STUDENT_REQUEST) {
                        debug(format("received STUDENT REQUEST {{ timestamp: {}, sender: {}, safehouse: {}, volume: {} }}"), message.timestamp, message.sender, message.payload.safehouse_index, message.payload.wine_volume);
                        send_ack(message);
                    }
                }

                trace(format("CHOSEN SAFEHOUSE: {}"), __safehouse);
                send_req();

                while (__ack_counter < __students_count - 1) {
                    auto message = Message::receive_from(ANY_SOURCE);
                    __timestamp = std::max(__timestamp, message.timestamp) + 1;
                    if (message.type == Message::Type::STUDENT_ACKNOWLEDGE && message.payload.last_timestamp == __priority) {
                        debug(format("received STUDENT ACKNOWLEDGE {{ timestamp: {}, sender: {}, safehouse: {}, request timestamp: {} }}"), message.timestamp, message.sender, message.payload.safehouse_index, message.payload.last_timestamp);
                        ++__ack_counter;
                    } else if (message.type == Message::Type::STUDENT_REQUEST) {
                        debug(format("received STUDENT REQUEST {{ timestamp: {}, sender: {}, safehouse: {}, volume: {} }}"), message.timestamp, message.sender, message.payload.safehouse_index, message.payload.wine_volume);
                        if (message.payload.safehouse_index != __safehouse) {
                            send_ack(message);
                        } else if (message.timestamp < __priority || (message.timestamp == __priority && message.sender < __rank)) {
                            send_ack(message);
                        } else if (message.timestamp > __priority || (message.timestamp == __priority && message.sender > __rank)) {
                            __pending_acks.emplace_back(message);
                            ++__ack_counter;
                        }
                        if (__safehouses[__safehouse] == 0) {
                            skip_safehouse = true;
                            break;
                        }
                    } else if (message.type == Message::Type::WINEMAKER_BROADCAST) {
                        debug(format("received WINEMAKER BROADCAST {{ timestamp: {}, sender: {}, safehouse: {}, volume: {} }}"), message.timestamp, message.sender, message.payload.safehouse_index, message.payload.wine_volume);
                        __safehouses[message.payload.safehouse_index] = message.payload.wine_volume;
                    }
                    trace(format("ACK COUNTER: {}"), __ack_counter);
                }

                if (skip_safehouse) {
                    skip_safehouse = false;
                    continue;
                }

                ++__timestamp;
                trace(format("safehouse acquire state {{ remaining demand: {}, safehouse #{} supplies: {} }}"), __demand, __safehouse, __safehouses[__safehouse]);
                const auto volume = std::min(static_cast<uint64_t>(__demand), __safehouses[__safehouse]);
                __safehouses[__safehouse] -= volume;
                __demand -= volume;
                trace(format("safehouse release state {{ remaining demand: {}, safehouse #{} supplies: {} }}"), __demand, __safehouse, __safehouses[__safehouse]);

                if (__safehouses[__safehouse] == 0) {
                    send_broadcast(__safehouse);
                }

                for(auto&& message : __pending_acks) {
                    send_ack(message);
                }
                __pending_acks.clear();
            }
        }
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

        __ack_counter = 0;
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