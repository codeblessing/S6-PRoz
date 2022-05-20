#include "student.hpp"

#include <algorithm>

#include <mpi/mpi.h>
#include <fmt/format.h>

#include "tags.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

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
    }

    auto Student::consume() -> void
    {
        __remaining_wine_demand = __dist(__rng);
        acquire_safe_place();
    }

    auto Student::handle_message(Message message) -> void
    {
    }

    auto Student::listen_for_messages() -> void
    {
#ifdef __WINEMAKER_TEST__
        int size;
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        // TODO: Fix this, so it can handla cases with > 2 processes.
        for (int i = 0; i < size; ++i)
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
            auto diff = static_cast<int64_t>(__safehouses[i]) - static_cast<int64_t>(__remaining_wine_demand);
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
        else if (min_negative_value > -__remaining_wine_demand)
        {
            __safehouse = index_min_negative;
            __acquiring_safehouse == true;
        }
        else
            __acquiring_safehouse = false;

        if (__acquiring_safehouse)
        {
            uint64_t buffer[3] = {++__timestamp, __safehouse, __remaining_wine_demand};
            for (auto receiver = __students[0]; receiver <= __students[1]; ++receiver)
                MPI_Send(&buffer, 3, MPI_LONG_LONG, receiver, STUDENT_ACQUIRE_REQ, MPI_COMM_WORLD);
        }
    }

    auto Student::broadcast(uint64_t safe_house) -> void
    {
    }

}

#pragma GCC diagnostic pop