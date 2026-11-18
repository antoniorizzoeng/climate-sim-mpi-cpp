#include <gtest/gtest.h>
#include <mpi.h>

#include "halo.hpp"
#include "init.hpp"

TEST(Volume, PeriodicHalosIncludeEdgesAndCorners) {
    SimConfig cfg;
    cfg.nx = 11;
    cfg.ny = 9;
    cfg.nz = 7;
    cfg.bc = {BCType::Periodic,
              BCType::Periodic,
              BCType::Periodic,
              BCType::Periodic,
              BCType::Periodic,
              BCType::Periodic};
    Decomposition dec;
    Field u = initialize_field(MPI_COMM_WORLD, dec, cfg);
    int ranks;
    MPI_Comm_size(MPI_COMM_WORLD, &ranks);
    EXPECT_EQ(dec.dims[0] * dec.dims[1] * dec.dims[2], ranks);
    auto value = [&](int i, int j, int k) {
        const int x = (dec.x_offset + i - 1 + cfg.nx) % cfg.nx;
        const int y = (dec.y_offset + j - 1 + cfg.ny) % cfg.ny;
        const int z = (dec.z_offset + k - 1 + cfg.nz) % cfg.nz;
        return x + cfg.nx * (y + cfg.ny * z);
    };
    u.fill(-1);
    for (int k = 1; k <= u.nz_local; ++k)
        for (int j = 1; j <= u.ny_local; ++j)
            for (int i = 1; i <= u.nx_local; ++i) u.at(i, j, k) = value(i, j, k);
    EXPECT_THROW(u.at(0, 0, u.nz_total()), std::out_of_range);
    exchange_halos(u, dec, dec.cart_comm);
    for (int k = 0; k < u.nz_total(); ++k)
        for (int j = 0; j < u.ny_total(); ++j)
            for (int i = 0; i < u.nx_total(); ++i) EXPECT_EQ(u.at(i, j, k), value(i, j, k));
    dec.finalize();
}

TEST(Volume, SingletonAxesStayWhole) {
    Decomposition dec;
    dec.init(MPI_COMM_WORLD, 1, 1, 13, false, false, true);
    int ranks;
    MPI_Comm_size(MPI_COMM_WORLD, &ranks);
    EXPECT_EQ(dec.dims[0], 1);
    EXPECT_EQ(dec.dims[1], 1);
    EXPECT_EQ(dec.dims[2], ranks);
    EXPECT_GE(dec.nz_local, 1);
    dec.finalize();
}
