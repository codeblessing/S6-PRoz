#pragma once

#include <cstdint>

namespace nouveaux
{
    struct Message
    {
        enum class Type
        {
            UNKNOWN,
            WMREQ,
            WMACK,
            WMINFO,
            STACK,
            STREQ,
            STINFO
        };

        union Content
        {
            // Winemaker REQ
            struct
            {
                uint64_t lamport_timestamp;
                uint64_t safehouse_index;
            } wm_req;

            // Winemaker/Student ACK
            struct
            {
                uint64_t lamport_timestamp;
            } ack;

            // Winemaker BROADCAST
            struct
            {
                uint64_t safehouse_index;
                uint64_t wine_volume;
            } wm_info;

            // Student REQ
            struct
            {
                uint64_t lamport_timestamp;
                uint64_t safehouse_index;
                uint64_t wine_volume;
            } st_req;

            // Student BROADCAST
            struct
            {
                uint64_t safehouse_index;
            } st_info;
        };

        Type type;
        uint64_t sender;
        Content content;
    };
}
