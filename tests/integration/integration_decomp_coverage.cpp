#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration, MPI_Decomposition_Coverage_NoOverlap) {
    fs::remove_all("outputs");
    fs::create_directories("outputs/snapshots");

    std::ostringstream cmd;
    cmd << MPIEXEC_EXECUTABLE << " " << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG << " "
        << INTEGRATION_MPI_PROCS << " " << CLIMATE_SIM_EXE << " --nx=64 --ny=48 --dx=1 --dy=1"
        << " --D=0 --vx=0 --vy=0"
        << " --dt=0.1 --steps=1 --out_every=1"
        << " --bc=periodic"
        << " --ic.mode=preset --ic.preset=constant_zero";
    ASSERT_EQ(run_cmd(cmd.str()), 0);

    auto tiles = read_rank_layout("outputs/rank_layout.csv");
    ASSERT_FALSE(tiles.empty());

    const int NX = tiles[0].nxg;
    const int NY = tiles[0].nyg;
    std::vector<char> covered((size_t)NX * NY, 0);

    for (const auto& t : tiles) {
        for (int j = 0; j < t.ny; ++j) {
            for (int i = 0; i < t.nx; ++i) {
                const int gi = t.x_off + i;
                const int gj = t.y_off + j;
                ASSERT_GE(gi, 0);
                ASSERT_LT(gi, NX);
                ASSERT_GE(gj, 0);
                ASSERT_LT(gj, NY);
                size_t idx = (size_t)gj * NX + gi;
                ASSERT_EQ(covered[idx], 0) << "overlap at (" << gi << "," << gj << ")";
                covered[idx] = 1;
            }
        }
    }

    for (size_t k = 0; k < covered.size(); ++k) {
        ASSERT_EQ(covered[k], 1) << "uncovered cell at linear index " << k;
    }
}