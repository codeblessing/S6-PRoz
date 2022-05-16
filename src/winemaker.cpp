#include "winemaker.hpp"
#include "tags.hpp"
#include <mpi/mpi.h>
#include <iostream>

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

    auto Winemaker::produce() -> void
    {
        acquire_safe_place();
    }

    auto Winemaker::handle_message(Message message) -> void
    {
        // If we get info that our safehouse has been freed & we're not trying to acquire it yet, now we do.
        if (message.type == Message::Type::STINFO && message.content.st_info.safehouse_index == __safehouse)
        {
            if (__safehouse_acquired)
            {
                __safehouse_acquired = false;
                for (auto &&receiver : __pending_ack)
                {
                    MPI_Send(nullptr, 0, MPI_BYTE, receiver, WINEMAKER_AQUIRE_ACK, MPI_COMM_WORLD);
                }
                
            }
            if (!__acquiring_safehouse)
                produce();
        }
        else if (message.type == Message::Type::WMREQ)
        {
            // If received request has lower priority (higher lamport clock) then we can treat this as ACK
            // but we need to remember to send ACK when we will free the safehouse.
            if (__acquiring_safehouse && message.content.wm_req.lamport_timestamp > __last_req_lamport)
            {
                ++__ack_counter;
                __pending_ack.push_back(message.sender);
            }
            else
            {
                MPI_Send(nullptr, 0, MPI_BYTE, message.sender, WINEMAKER_AQUIRE_ACK, MPI_COMM_WORLD);
            }
        }
        else if (message.type == Message::Type::WMACK && __acquiring_safehouse)
        {
            ++__ack_counter;
        }

        // If we hit all ACKs we're holding safehouse.
        if (__ack_counter == __winemakers_count)
        {
            __acquiring_safehouse = false;
            __safehouse_acquired = true;
            broadcast(__dist(__rng), __safehouse);
        }
    }

    auto Winemaker::acquire_safe_place() -> std::size_t
    {
        __acquiring_safehouse = true;

        const uint64_t message[2] = {++__lamport, __safehouse};

        // Send REQ message for all winemakers.
        for (int receiver = __winemakers[0]; receiver <= __winemakers[1]; ++receiver)
            MPI_Send(&message, 2, MPI_LONG_LONG, receiver, WINEMAKER_ACQUIRE_REQ, MPI_COMM_WORLD);
    }

    auto Winemaker::broadcast(uint32_t volume, std::size_t safe_house) -> void
    {
        for (auto receiver = 0; receiver < __system_size; ++receiver)
        {
            if (receiver != __rank)
                MPI_Send(&volume, 1, MPI_UINT32_T, receiver, WINEMAKER_BROADCAST, MPI_COMM_WORLD);
        }
    }
}

// #define __WINEMAKER_TEST__

#ifdef __WINEMAKER_TEST__

#include <doctest/doctest.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

using namespace nouveaux;

TEST_SUITE("winemaker::Winemaker")
{
    TEST_CASE("produce")
    {
        auto maker = Winemaker(10, {0, 0}, 1, 10);
        maker.produce();
        auto safe_houses = maker.safe_houses();

        fmt::print("[{}]\n", fmt::join(safe_houses, ", "));

        bool exactly_one_safe_house_has_wine = false;
        for (decltype(safe_houses.size()) index = 0; index < safe_houses.size(); ++index)
        {
            if (safe_houses[index] != 0 && exactly_one_safe_house_has_wine == false)
            {
                exactly_one_safe_house_has_wine = true;
            }
            else if (safe_houses[index] != 0)
            {
                exactly_one_safe_house_has_wine = false;
                break;
            }
        }

        CHECK(exactly_one_safe_house_has_wine);
    }
}

#endif