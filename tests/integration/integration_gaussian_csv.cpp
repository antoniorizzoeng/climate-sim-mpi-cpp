#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration, GaussianPresetCSV_NontrivialRange) {
    fs::remove_all("outputs");
    fs::create_directories("outputs/snapshots");

    std::ostringstream cmd;
    cmd << MPIEXEC_EXECUTABLE << " " << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG << " "
        << INTEGRATION_MPI_PROCS << " " << CLIMATE_SIM_EXE << " --nx=64 --ny=64 --dx=1 --dy=1"
        << " --D=0 --vx=0 --vy=0"
        << " --dt=0.1 --steps=1 --out_every=1"
        << " --bc=periodic"
        << " --ic.mode=preset --ic.preset=gaussian_hotspot --ic.A=1.0 --ic.sigma_frac=0.1";
    ASSERT_EQ(run_cmd(cmd.str()), 0);

    auto grid = assemble_global_csv_snapshot(0);
    ASSERT_FALSE(grid.empty());
    ASSERT_EQ(grid.size(), 64u);
    ASSERT_EQ(grid[0].size(), 64u);

    double mn = 1e300, mx = -1e300;
    for (const auto& row : grid)
        for (double v : row) {
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }

    EXPECT_GE(mn, 0.0);
    EXPECT_LE(mx, 1.0);
    EXPECT_GT(mx, 1e-6);
}