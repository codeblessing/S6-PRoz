#include "winemaker.hpp"
#include "tags.hpp"
#include <mpi/mpi.h>
#include <iostream>
#include <cmath>
#include <fmt/format.h>
#include <fmt/ranges.h>

namespace nouveaux
{
    Winemaker::Builder::Builder(uint64_t safehouse_count, std::array<uint64_t, 2> &&winemakers)
        : min_wine_volume(1), max_wine_volume(1000), winemakers(std::move(winemakers))
    {
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &system_size);

        safehouse = rank % safehouse_count;
    }

    auto Winemaker::Builder::wine_volume(uint32_t min_volume, uint32_t max_volume) -> Builder
    {
        // Swap values if they're in wrong order.
        if (min_volume > max_volume)
        {
            auto temp = min_volume;
            min_volume = max_volume;
            max_volume = temp;
        }

        min_wine_volume = min_volume;
        max_wine_volume = max_volume;

        return *this;
    }

    auto Winemaker::Builder::build() -> Winemaker
    {
        return Winemaker(rank, system_size, safehouse, std::move(winemakers), min_wine_volume, max_wine_volume);
    }

    Winemaker::Winemaker(int32_t rank, int32_t system_size, uint64_t safehouse, std::array<uint64_t, 2> &&winemakers, uint32_t min_wine_volume, uint32_t max_wine_volume)
        : __rng(std::random_device()()),
          __dist(min_wine_volume, max_wine_volume),
          __safehouse(safehouse),
          __winemakers(std::move(winemakers)),
          __winemakers_count(__winemakers[1] - __winemakers[0]),
          __rank(rank),
          __system_size(system_size)
    {
    }

    auto Winemaker::builder(uint64_t safehouse_count, std::array<uint64_t, 2> &&winemakers) -> Builder
    {
        return Winemaker::Builder(safehouse_count, std::move(winemakers));
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
#ifdef __WINEMAKER_TEST__
        int size;
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        for (int i = 0; i < 2 * size - 2; ++i)
#else
        while (true)
#endif
        {
            if (__winemakers_count > 0)
            {
                uint64_t buffer[3];
                MPI_Status status;
                MPI_Recv(&buffer, 3, MPI_LONG_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

                Message::Type type;
                if (status.MPI_TAG == WINEMAKER_ACQUIRE_REQ)
                    type = Message::Type::WMREQ;
                else if (status.MPI_TAG == WINEMAKER_ACQUIRE_ACK)
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
                    memcpy(&content.wm_ack, &buffer, sizeof(uint64_t));
                }
                else if (type == Message::Type::STINFO)
                {
                    memcpy(&content.st_info, &buffer, sizeof(uint64_t) * 2);
                }

                Message message{type, sender, content};
                handle_message(message);
            }
            else
            {
                handle_message(Message{});
            }
        }
    }

    auto Winemaker::handle_message(Message message) -> void
    {
        // If we get info that our safehouse has been freed & we're not trying to acquire it yet, now we do.
        if (message.type == Message::Type::STINFO && message.content.st_info.safehouse_index == __safehouse)
        {
            if (__safehouse_acquired)
            {
                fmt::print("Winemaker #{} freed safehouse {}.\n", __rank, __safehouse);
                __safehouse_acquired = false;
                ++__timestamp;
                for (auto &&receiver : __pending_ack)
                {
                    MPI_Send(&__timestamp, 1, MPI_LONG_LONG, receiver, WINEMAKER_ACQUIRE_ACK, MPI_COMM_WORLD);
                }
            }
            if (!__acquiring_safehouse)
                produce();
        }
        else if (message.type == Message::Type::WMREQ)
        {
            // If received request has lower priority (higher lamport clock) then we can treat this as ACK
            // but we need to remember to send ACK when we will free the safehouse.
            if (__acquiring_safehouse && message.content.wm_req.lamport_timestamp > __priority && message.content.wm_req.safehouse_index == __safehouse)
            {
                __timestamp = std::max(__timestamp, message.content.wm_req.lamport_timestamp);
                ++__ack_counter;
                __pending_ack.push_back(message.sender);
            }
            else
            {
                ++__timestamp;
                MPI_Send(&__timestamp, 1, MPI_LONG_LONG, message.sender, WINEMAKER_ACQUIRE_ACK, MPI_COMM_WORLD);
            }
        }
        else if (message.type == Message::Type::WMACK && __acquiring_safehouse)
        {
            __timestamp = std::max(__timestamp, message.content.wm_ack.lamport_timestamp);
            ++__ack_counter;
        }

        // If we hit all ACKs we're holding safehouse.
        if (__ack_counter == __winemakers_count)
        {
            fmt::print("Winemaker #{} acquired safehouse {}.\n", __rank, __safehouse);
            __acquiring_safehouse = false;
            __safehouse_acquired = true;
            __ack_counter = 0;
            broadcast(__dist(__rng), __safehouse);
        }
    }

    auto Winemaker::acquire_safe_place() -> void
    {
        __acquiring_safehouse = true;

        const uint64_t message[2] = {++__timestamp, __safehouse};
        __priority = __timestamp;

        // Send REQ message for all winemakers.
        for (auto receiver = __winemakers[0]; receiver <= __winemakers[1]; ++receiver)
            if ((int)receiver != __rank)
                MPI_Send(&message, 2, MPI_LONG_LONG, receiver, WINEMAKER_ACQUIRE_REQ, MPI_COMM_WORLD);
    }

    auto Winemaker::broadcast(uint32_t volume, uint64_t safe_house) -> void
    {
        fmt::print("Winemaker #{} stores {} wine units in {}.\n", __rank, volume, __safehouse);
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

using namespace nouveaux;

TEST_SUITE("winemaker::Winemaker")
{
    TEST_CASE("produce")
    {
        int size;
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        auto maker = Winemaker::builder(10, {0, size - 1}).wine_volume(1, 10).build();
        maker.run();
        CHECK(maker.__safehouse_acquired);
        CHECK(maker.__ack_counter == 0);
        CHECK(maker.__timestamp >= size);
        CHECK(maker.__priority == 1);
    }
}

#endif