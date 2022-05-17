#ifdef __WINEMAKER_TEST__

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.hpp>
#include <mpi/mpi.h>

int main(int argc, char **argv)
{
    doctest::Context context;

    context.applyCommandLine(argc, argv);

    // context.setOption("no-breaks", true); // don't break in the debugger when assertions fail

    MPI_Init(&argc, &argv);

    int status = context.run(); // run

    MPI_Finalize();

    if (context.shouldExit()) // important - query flags (and --exit) rely on the user doing this
        return status;        // propagate the result of the tests

    return status;
}

#else

#include <mpi/mpi.h>
#include "winemaker.hpp"
#include <iostream>

int main()
{
    std::cout << "Hello" << std::endl;
}

#endif