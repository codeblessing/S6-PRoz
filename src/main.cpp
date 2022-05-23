#include <fmt/format.h>
#include <fmt/ranges.h>
#include <mpi/mpi.h>

#include "student.hpp"
#include "winemaker.hpp"

using namespace nouveaux;

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    uint32_t rank;
    uint32_t size;
    MPI_Comm_rank(MPI_COMM_WORLD, reinterpret_cast<int*>(&rank));
    MPI_Comm_size(MPI_COMM_WORLD, reinterpret_cast<int*>(&size));

    if (size < 2) {
        fmt::print(stderr, "At least 2 processes are required for program to work correctly. Aborting.\n");
        return -1;
    }

    const uint64_t winemakers_count = static_cast<uint64_t>(size / 3 + 1);
    const uint64_t students_count = static_cast<uint64_t>(size - winemakers_count);
    const uint64_t safehouse_count = winemakers_count > 1 ? winemakers_count / 2 : 1;
    if (static_cast<uint64_t>(rank) < winemakers_count) {
        fmt::print("Spawning winemaker #{}.\n", rank);
        auto winemaker = Winemaker(safehouse_count, rank, winemakers_count, students_count, 0, winemakers_count, 100, 1000);

#ifdef __WINEMAKER_DEBUG__
        // !!! DEBUG !!!
        if (rank == 0) {
            fmt::print("Safehouse count: {}\n", safehouse_count);
            fmt::print("Winemakers count: {}\n", winemaker.__winemakers_count);
        }
        // !!! /DEBUG !!!
#endif
        winemaker.run();
    } else {
        fmt::print("Spawning student #{}.\n", rank);
        auto student = Student(safehouse_count, rank, winemakers_count, students_count, 0, winemakers_count, 1, 100);
#ifdef __WINEMAKER_DEBUG__
        if (rank == winemakers_count) {
            fmt::print("Safehouses: {}\n", student.__safehouses);
        }
#endif
        student.run();
    }
}
