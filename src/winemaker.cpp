#include "winemaker.hpp"
#include "tags.hpp"
#include <mpi/mpi.h>
#include <iostream>
#include <cmath>
#include <fmt/format.h>
#include <fmt/ranges.h>

namespace nouveaux
{
    Winemaker::Winemaker(std::size_t safe_house_count, std::array<uint32_t, 2> winemakers_index_range, uint32_t min_wine_volume, uint32_t max_wine_volume)
        : __rng(std::random_device()()), __dist(min_wine_volume, max_wine_volume), __winemakers(winemakers_index_range)
    {
        MPI_Comm_rank(MPI_COMM_WORLD, &__rank);
        MPI_Comm_size(MPI_COMM_WORLD, &__system_size);

        __safehouse = __rank % safe_house_count;
        __winemakers_count = winemakers_index_range[1] - winemakers_index_range[0];
    }

    auto Winemaker::run() -> void
    {
        produce();
        listen_for_messages();
    }

    auto Winemaker::produce() -> void
    {
        acquire_safe_place();
    }

    auto Winemaker::listen_for_messages() -> void
    {
        fmt::print("Entering listen_for_messages.\n");
        #ifdef __WINEMAKER_TEST__
        for(int i = 0; i < 2; ++i)
        #else
        while (true)
        #endif
        {
            uint64_t buffer[3];
            MPI_Status status;
            MPI_Recv(&buffer, 3, MPI_LONG_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            Message::Type type;
            if (status.MPI_TAG == WINEMAKER_ACQUIRE_REQ)
                type = Message::Type::WMREQ;
            else if (status.MPI_TAG == WINEMAKER_AQUIRE_ACK)
                type = Message::Type::WMACK;
            else if (status.MPI_TAG == STUDENT_BROADCAST)
                type = Message::Type::STINFO;
            else
                type = Message::Type::UNKNOWN;

            uint64_t sender = status.MPI_SOURCE;
            Message::Content content;
            if (type == Message::Type::WMREQ)
            {
                memcpy(&content.wm_req, &buffer, sizeof(uint64_t) * 2);
            }
            else if (type == Message::Type::WMACK)
            {
                memcpy(&content.ack, &buffer, sizeof(uint64_t));
            }
            else if (type == Message::Type::STINFO)
            {
                memcpy(&content.st_info, &buffer, sizeof(uint64_t));
            }

            Message message{
                type,
                sender,
                content
            };
            fmt::print("Leaving listen_for_messages.\n");

            handle_message(message);
        }
    }

    auto Winemaker::handle_message(Message message) -> void
    {
        fmt::print("Entering handle_message.\n");
        // If we get info that our safehouse has been freed & we're not trying to acquire it yet, now we do.
        if (message.type == Message::Type::STINFO && message.content.st_info.safehouse_index == __safehouse)
        {
            if (__safehouse_acquired)
            {
                __safehouse_acquired = false;
                ++__lamport;
                for (auto &&receiver : __pending_ack)
                {
                    MPI_Send(&__lamport, 1, MPI_LONG_LONG, receiver, WINEMAKER_AQUIRE_ACK, MPI_COMM_WORLD);
                }
            }
            if (!__acquiring_safehouse)
                produce();
        }
        else if (message.type == Message::Type::WMREQ)
        {
            // If received request has lower priority (higher lamport clock) then we can treat this as ACK
            // but we need to remember to send ACK when we will free the safehouse.
            if (__acquiring_safehouse && message.content.wm_req.lamport_timestamp > __last_req_lamport && message.content.wm_req.safehouse_index == __safehouse)
            {
                fmt::print("Received WMREQ from {}, waiting...\n", message.sender);
                __lamport = std::max(__lamport, message.content.wm_req.lamport_timestamp);
                ++__ack_counter;
                __pending_ack.push_back(message.sender);
            }
            else
            {
                fmt::print("Received WMREQ from {}, sending ACK\n", message.sender);
                ++__lamport;
                MPI_Send(&__lamport, 1, MPI_LONG_LONG, message.sender, WINEMAKER_AQUIRE_ACK, MPI_COMM_WORLD);
            }
        }
        else if (message.type == Message::Type::WMACK && __acquiring_safehouse)
        {
            fmt::print("Received WMACK from {}\n", message.sender);
            __lamport = std::max(__lamport, message.content.ack.lamport_timestamp);
            ++__ack_counter;
        }

        // If we hit all ACKs we're holding safehouse.
        fmt::print("ACK counter: {}, Winemakers count: {}\n", __ack_counter, __winemakers_count);
        if (__ack_counter == __winemakers_count)
        {
            fmt::print("All ACKs acquired.\n");
            __acquiring_safehouse = false;
            __safehouse_acquired = true;
            __ack_counter = 0;
            broadcast(__dist(__rng), __safehouse);
        }
        fmt::print("Leaving handle_message.\n");
    }

    auto Winemaker::acquire_safe_place() -> void
    {
        fmt::print("Entering acquire_safe_place().\n");
        __acquiring_safehouse = true;
        fmt::print("__acquiring_safehouse set to true.\n");

        const uint64_t message[2] = {++__lamport, __safehouse};
        __last_req_lamport = __lamport;

        fmt::print("Message: ({}, {})\n", message[0], message[1]);

        // Send REQ message for all winemakers.
        for (auto receiver = __winemakers[0]; receiver <= __winemakers[1]; ++receiver)
            if ((int)receiver != __rank)
                MPI_Send(&message, 2, MPI_LONG_LONG, receiver, WINEMAKER_ACQUIRE_REQ, MPI_COMM_WORLD);

        fmt::print("All messages sent.\nLeaving acquire_safe_place().\n");
    }

    auto Winemaker::broadcast(uint32_t volume, uint64_t safe_house) -> void
    {
        const uint64_t message[2] = {safe_house, volume};
        for (auto receiver = 0; receiver < __system_size; ++receiver)
        {
            if (receiver != __rank)
                MPI_Send(&message, 2, MPI_UINT64_T, receiver, WINEMAKER_BROADCAST, MPI_COMM_WORLD);
        }
    }
}

#ifdef __WINEMAKER_TEST__

#include <doctest/doctest.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

using namespace nouveaux;

TEST_SUITE("winemaker::Winemaker")
{
    TEST_CASE("produce")
    {
        fmt::print("Creating winemaker.\n");
        auto maker = Winemaker(10, {0, 1}, 1, 10);
        fmt::print("Entering command section.");
        maker.run();
        fmt::print("maker.produce() finished.\n");
        fmt::print("Entering assertions section.\n");
        CHECK(maker.__safehouse_acquired);
        CHECK(maker.__ack_counter == 0);
        CHECK(maker.__lamport == 2);
        CHECK(maker.__last_req_lamport == 1);
        fmt::print("Leaving assertions section.\n");
    }
}

#endif