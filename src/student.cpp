#include "student.hpp"

#include <algorithm>

#include <mpi/mpi.h>
#include <fmt/format.h>

#include "tags.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

// TODO 1: Check & fix UBs connected with unsigned overflows (especially in __safehouses).

namespace nouveaux
{

    Student::Student(uint64_t safehouse_count, int32_t rank, int32_t system_size, std::array<uint64_t, 2> &&students, uint32_t min_wine_volume, uint32_t max_wine_volume)
        : __rng(std::random_device()()),
          __dist(min_wine_volume, max_wine_volume),
          __students(std::move(students)),
          __students_count(__students[1] - __students[0]),
          __rank(rank),
          __system_size(system_size)
    {
        __safehouses.reserve(safehouse_count);
        for (uint64_t i = 0; i < safehouse_count; ++i)
        {
            __safehouses.push_back(0);
        }
    }

    auto Student::run() -> void
    {
        consume();
        listen_for_messages();
    }

    auto Student::consume() -> void
    {
        if (__demand <= 0)
            __demand = __dist(__rng);
        acquire_safe_place();
    }

    auto Student::handle_message(Message message) -> void
    { // If we get info that our safehouse has been freed & we're not trying to acquire it yet, now we do.
        __timestamp = std::max(__timestamp, message.content.st_req.lamport_timestamp);
        if (message.type == Message::Type::WMINFO)
        {
            __safehouses[message.content.wm_info.safehouse_index] = message.content.wm_info.wine_volume;
        }
        else if (message.type == Message::Type::STREQ)
        {
            auto [timestamp, index, volume] = message.content.st_req;
            // If received request has lower priority (higher lamport clock) then we can treat this as ACK
            // but we need to remember to send ACK when we will free the safehouse.
            if (__acquiring_safehouse && index == __safehouse)
            {
                if (timestamp > __priority)
                {
                    ++__ack_counter;
                    __pending_ack.emplace_back(message.sender, index);
                }
                else
                {
                    __safehouses[index] -= volume;
                    if (__safehouses[index] < 1)
                    {
                        // This one has taken all wine from our safehouse, so we
                        // invalidate our current acquisition and request other safehouse.
                        __ack_counter = 0;
                        consume();
                    }
                }
            }
            else
            {
                uint64_t buffer[2] = {++__timestamp, index};
                MPI_Send(&buffer, 2, MPI_LONG_LONG, message.sender, WINEMAKER_ACQUIRE_ACK, MPI_COMM_WORLD);
            }
        }
        else if (message.type == Message::Type::STACK && __acquiring_safehouse)
        {
            if (message.content.st_ack.safehouse_index == __safehouse)
                ++__ack_counter;
        }

        // If we hit all ACKs we're holding safehouse.
        if (__ack_counter == __students_count)
        {
            fmt::print("Student #{} acquired safehouse {}.\n", __rank, __safehouse);
            __acquiring_safehouse = false;
            __ack_counter = 0;
            // TODO 2: Implement logic for mutating state of wine demand.

            // broadcast(__safehouse);
        }
    }

    auto Student::listen_for_messages() -> void
    {
#ifdef __WINEMAKER_TEST__
        int size;
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        for (int i = 0; i < 2 * size - 2; ++i)
#else
        while (true)
#endif
        {
            if (__students_count > 0)
            {
                uint64_t buffer[3];
                MPI_Status status;
                MPI_Recv(&buffer, 3, MPI_LONG_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

                Message::Type type;
                if (status.MPI_TAG == STUDENT_ACQUIRE_REQ)
                    type = Message::Type::STREQ;
                else if (status.MPI_TAG == STUDENT_ACQUIRE_ACK)
                    type = Message::Type::STACK;
                else if (status.MPI_TAG == WINEMAKER_BROADCAST)
                    type = Message::Type::WMINFO;
                else
                    type = Message::Type::UNKNOWN;

                uint64_t sender = status.MPI_SOURCE;
                Message::Content content;
                if (type == Message::Type::STREQ)
                {
                    memcpy(&content.st_req, &buffer, sizeof(uint64_t) * 3);
                }
                else if (type == Message::Type::STACK)
                {
                    memcpy(&content.st_ack, &buffer, sizeof(uint64_t) * 2);
                }
                else if (type == Message::Type::WMINFO)
                {
                    memcpy(&content.wm_info, &buffer, sizeof(uint64_t) * 3);
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

    auto Student::acquire_safe_place() -> void
    {
        int index_min_nonnegative = 0;
        int index_min_negative = 0;
        int64_t min_negative_value = std::numeric_limits<int64_t>::max();
        int64_t min_nonnegative_value = std::numeric_limits<int64_t>::min();
        for (auto i = 0; i < __safehouses.size(); ++i)
        {
            auto diff = static_cast<int64_t>(__safehouses[i]) - static_cast<int64_t>(__demand);
            if (diff < 0 && diff > min_negative_value)
            {
                min_negative_value = diff;
                index_min_negative = i;
            }
            else if (diff >= 0 && diff < min_nonnegative_value)
            {
                min_nonnegative_value = diff;
                index_min_nonnegative = i;
            }
        }

        // If there's a safehouse that can satisfy our demand, we choose this one.
        if (min_nonnegative_value > -1)
        {
            __safehouse = index_min_nonnegative;
            __acquiring_safehouse = true;
        }
        // Otherwise we choose the one with minimal deficiency (unless this deficiency is lower than our demand).
        else if (min_negative_value > -__demand)
        {
            __safehouse = index_min_negative;
            __acquiring_safehouse == true;
        }
        else
            __acquiring_safehouse = false;

        if (__acquiring_safehouse)
        {
            uint64_t buffer[3] = {++__timestamp, __safehouse, __demand};
            __priority = __timestamp;
            for (auto receiver = __students[0]; receiver <= __students[1]; ++receiver)
                MPI_Send(&buffer, 3, MPI_LONG_LONG, receiver, STUDENT_ACQUIRE_REQ, MPI_COMM_WORLD);
        }
    }

    auto Student::broadcast(uint64_t safe_house) -> void
    {
    }

}

#pragma GCC diagnostic pop