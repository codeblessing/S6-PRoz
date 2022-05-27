#include <fmt/format.h>
#include <fmt/ranges.h>
#include <mpi/mpi.h>

#include "config.hpp"
#include "logger.hpp"
#include "student.hpp"
#include "winemaker.hpp"

using namespace nouveaux;

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    uint32_t rank;
    uint32_t size;
    MPI_Comm_rank(MPI_COMM_WORLD, reinterpret_cast<int*>(&rank));
    MPI_Comm_size(MPI_COMM_WORLD, reinterpret_cast<int*>(&size));

    const auto config = Config::parse("config.toml");
    if (size < (config.winemaker_count + config.student_count)) {
        fmt::print(stderr, "At least {} processes are required for program to work correctly with current configuration. Aborting.\n", (config.winemaker_count + config.student_count));
        return -1;
    }

    Logger::init(rank);

    if (static_cast<uint64_t>(rank) < config.winemaker_count) {
        trace("Spawning winemaker #{}.\n", rank);
        auto winemaker = Winemaker(config.safehouse_count, rank, config.winemaker_count, config.student_count, 0, config.winemaker_count, config.min_wine_volume, config.max_wine_volume);

#ifdef NOUVEAUX_DEBUG
        if (rank == 0) {
            trace("Safehouse count: {}\n", config.safehouse_count);
            trace("Winemakers count: {}\n", winemaker.__winemakers_count);
        }
#endif

        winemaker.run();
    } else {
        trace("Spawning student #{}.\n", rank);
        auto student = Student(config.safehouse_count, rank, config.winemaker_count, config.student_count, 0, config.winemaker_count, config.min_wine_volume, config.max_wine_volume);
        student.run();
    }

    MPI_Finalize();
}
