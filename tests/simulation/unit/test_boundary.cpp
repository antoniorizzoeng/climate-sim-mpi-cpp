#include <gtest/gtest.h>
#include <mpi.h>

#include "boundary.hpp"
#include "decomp.hpp"
#include "field.hpp"

TEST(Unit_Boundary, DirichletAndNeumannSingleRank) {
    int init = 0;
    MPI_Initialized(&init);
    if (!init) {
        int prov = 0;
        MPI_Init_thread(nullptr, nullptr, MPI_THREAD_FUNNELED, &prov);
    }

    int size = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (size != 1)
        GTEST_SKIP() << "Run boundary single-rank test with -np 1";

    const int NX = 4, NY = 3;
    Decomp2D dec;
    dec.init(MPI_COMM_WORLD, NX, NY);

    const int h = 1;
    Field f(NX, NY, h, 1.0, 1.0);
    f.fill(-1.0);
    for (int j = h; j < h + NY; ++j)
        for (int i = h; i < h + NX; ++i) f.at(i, j) = 10.0;

    apply_boundary(f, dec, BCType::Dirichlet, 5.0);
    for (int j = 0; j < f.ny_total(); ++j) {
        EXPECT_DOUBLE_EQ(f.at(0, j), 5.0);
        EXPECT_DOUBLE_EQ(f.at(h + NX, j), 5.0);
    }
    for (int i = 0; i < f.nx_total(); ++i) {
        EXPECT_DOUBLE_EQ(f.at(i, 0), 5.0);
        EXPECT_DOUBLE_EQ(f.at(i, h + NY), 5.0);
    }

    f.fill(-1.0);
    for (int j = h; j < h + NY; ++j)
        for (int i = h; i < h + NX; ++i) f.at(i, j) = static_cast<double>(j);

    apply_boundary(f, dec, BCType::Neumann);

    for (int j = 0; j < f.ny_total(); ++j) {
        EXPECT_DOUBLE_EQ(f.at(0, j), f.at(h, j));
        EXPECT_DOUBLE_EQ(f.at(h + NX, j), f.at(h + NX - 1, j));
    }
    for (int i = 0; i < f.nx_total(); ++i) {
        EXPECT_DOUBLE_EQ(f.at(i, 0), f.at(i, h));
        EXPECT_DOUBLE_EQ(f.at(i, h + NY), f.at(i, h + NY - 1));
    }

    dec.finalize();
    int fin = 0;
    MPI_Finalized(&fin);
    if (!fin)
        MPI_Finalize();
}
