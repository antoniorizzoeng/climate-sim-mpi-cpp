#include <gtest/gtest.h>
#include <mpi.h>
#include "decomp.hpp"
#include "field.hpp"
#include "halo.hpp"

TEST(HaloExchange, FourWayOneCell)
{
    int provided = 0;
    int initialized = MPI_Initialized(&initialized), finalized = 0;
    if (!initialized) MPI_Init_thread(nullptr, nullptr, MPI_THREAD_FUNNELED, &provided);

    int rank=0, size=0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (size < 4) GTEST_SKIP() << "requires at least 4 ranks";

    const int nxg = 8, nyg = 8;
    Decomp2D dec;
    dec.init(MPI_COMM_WORLD, nxg, nyg);

    const int h = 1;
    Field f(dec.nx_local, dec.ny_local, h, 1.0, 1.0);

    f.fill(-1.0);
    for (int j = h; j < h + dec.ny_local; ++j)
        for (int i = h; i < h + dec.nx_local; ++i)
            f.at(i,j) = static_cast<double>(rank);

    exchange_halos(f, dec, MPI_COMM_WORLD);

    if (dec.nbr_lr[0] != MPI_PROC_NULL) {
        for (int j = h; j < h + dec.ny_local; ++j)
            EXPECT_EQ(f.at(0, j), static_cast<double>(dec.nbr_lr[0])) << "left halo mismatch";
    }
    if (dec.nbr_lr[1] != MPI_PROC_NULL) {
        for (int j = h; j < h + dec.ny_local; ++j)
            EXPECT_EQ(f.at(h + dec.nx_local, j), static_cast<double>(dec.nbr_lr[1])) << "right halo mismatch";
    }
    if (dec.nbr_du[0] != MPI_PROC_NULL) {
        for (int i = h; i < h + dec.ny_local; ++i)
            EXPECT_EQ(f.at(i, 0), static_cast<double>(dec.nbr_du[0])) << "bottom halo mismatch";
    }
    if (dec.nbr_du[1] != MPI_PROC_NULL) {
        for (int i = h; i < h + dec.nx_local; ++i)
            EXPECT_EQ(f.at(i, h + dec.ny_local), static_cast<double>(dec.nbr_du[1])) << "top halo mismatch";
    }

    dec.finalize();
    MPI_Finalized(&finalized);
    if (!finalized) MPI_Finalize();
}
