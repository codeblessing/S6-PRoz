#pragma once

#include <cstdint>
#include <cstring>
#include <mpi.h>

#include "tags.hpp"

namespace nouveaux
{
    struct Message
    {
        enum struct Type : uint64_t
        {
            UNKNOWN,
            WINEMAKER_REQUEST,
            WINEMAKER_ACKNOWLEDGE,
            WINEMAKER_BROADCAST,
            STUDENT_REQUEST,
            STUDENT_ACKNOWLEDGE,
            STUDENT_BROADCAST
        };

        struct Payload
        {
            uint64_t safehouse_index;
            uint64_t wine_volume;
            uint64_t last_timestamp;
        };

        Type type;
        // Sender rank.
        uint64_t sender;
        // Lamport timestamp.
        uint64_t timestamp;
        // If message contains additional content (eg. REQ)
        // appropriate fields in this struct will be filled.
        Payload payload;

        auto send_to(uint64_t receiver) -> void
        {
            uint64_t message[4] = {timestamp, payload.safehouse_index, payload.wine_volume, payload.last_timestamp};
            uint64_t tag = 0;
            uint64_t size = 0;

            switch (type)
            {
            case Type::WINEMAKER_REQUEST:
                tag = WINEMAKER_ACQUIRE_REQ;
                size = 2;
                break;
            case Type::WINEMAKER_ACKNOWLEDGE:
                tag = WINEMAKER_ACQUIRE_ACK;
                size = 1;
                break;
            case Type::WINEMAKER_BROADCAST:
                tag = WINEMAKER_BROADCAST;
                size = 3;
                break;
            case Type::STUDENT_REQUEST:
                tag = STUDENT_ACQUIRE_REQ;
                size = 3;
                break;
            case Type::STUDENT_ACKNOWLEDGE:
                tag = STUDENT_ACQUIRE_ACK;
                size = 4;
                break;
            case Type::STUDENT_BROADCAST:
                tag = STUDENT_BROADCAST;
                size = 2;
                break;
            default:
                tag = UNKNOWN;
                break;
            }

            if (tag != UNKNOWN)
                MPI_Send(&message, size, MPI_LONG_LONG, receiver, tag, MPI_COMM_WORLD);
        }

        static auto receive_from(uint64_t sender) -> Message
        {
            uint64_t message[4] = {0};
            MPI_Status status;
            MPI_Recv(&message, 4, MPI_LONG_LONG, sender, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            Type type;
            switch (status.MPI_TAG)
            {
            case WINEMAKER_ACQUIRE_REQ:
                type = Type::WINEMAKER_REQUEST;
                break;
            case WINEMAKER_ACQUIRE_ACK:
                type = Type::WINEMAKER_ACKNOWLEDGE;
                break;
            case WINEMAKER_BROADCAST:
                type = Type::WINEMAKER_BROADCAST;
                break;
            case STUDENT_ACQUIRE_REQ:
                type = Type::STUDENT_REQUEST;
                break;
            case STUDENT_ACQUIRE_ACK:
                type = Type::STUDENT_ACKNOWLEDGE;
                break;
            case STUDENT_BROADCAST:
                type = Type::STUDENT_BROADCAST;
                break;
            default:
                type = Type::UNKNOWN;
                break;
            }

            uint64_t message_sender = status.MPI_SOURCE;
            uint64_t timestamp = message[0];
            Payload payload{};
            memcpy(&payload, &message[1], sizeof(uint64_t) * 3);

            return Message{
                type,
                message_sender,
                timestamp,
                payload
            };
        }
    };
}
