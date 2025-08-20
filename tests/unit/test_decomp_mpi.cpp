#include <gtest/gtest.h>
#include <mpi.h>

#include "decomp.hpp"

TEST(Unit_Decomp, GridDimsAndNeighbors) {
    int world_size = 0, world_rank = -1;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    Decomp2D d;
    d.init(MPI_COMM_WORLD, 16, 12);

    ASSERT_EQ(d.dims[0] * d.dims[1], world_size);
    EXPECT_GE(d.coords[0], 0);
    EXPECT_GE(d.coords[1], 0);
    EXPECT_LT(d.coords[0], d.dims[0]);
    EXPECT_LT(d.coords[1], d.dims[1]);

    const bool on_left = (d.coords[0] == 0);
    const bool on_right = (d.coords[0] == d.dims[0] - 1);
    const bool on_down = (d.coords[1] == 0);
    const bool on_up = (d.coords[1] == d.dims[1] - 1);

    if (!on_left)
        EXPECT_NE(d.nbr_lr[0], MPI_PROC_NULL);
    if (!on_right)
        EXPECT_NE(d.nbr_lr[1], MPI_PROC_NULL);
    if (!on_down)
        EXPECT_NE(d.nbr_du[0], MPI_PROC_NULL);
    if (!on_up)
        EXPECT_NE(d.nbr_du[1], MPI_PROC_NULL);

    d.finalize();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    MPI_Init(&argc, &argv);
    const int rc = RUN_ALL_TESTS();
    MPI_Finalize();
    return rc;
}
