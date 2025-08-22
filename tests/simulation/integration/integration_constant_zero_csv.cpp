#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration, ConstantZeroCSV_Step0_AllZeros) {
    std::ostringstream cmd;
    cmd << MPIEXEC_EXECUTABLE << " " << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG << " "
        << INTEGRATION_MPI_PROCS << " " << CLIMATE_SIM_EXE << " --nx=32 --ny=32 --dx=1 --dy=1"
        << " --D=0 --vx=0 --vy=0"
        << " --dt=0.1 --steps=1 --out_every=1"
        << " --bc=periodic"
        << " --ic.mode=preset --ic.preset=constant_zero";

    ASSERT_EQ(run_cmd(cmd.str()), 0) << "main run failed";

    fs::path snap = fs::path("outputs/snapshots") / "snapshot_00000_rank00000.csv";
    ASSERT_TRUE(fs::exists(snap)) << "missing " << snap;

    auto grid = read_csv_2d(snap);
    ASSERT_FALSE(grid.empty());
    for (const auto& row : grid) {
        for (double v : row) {
            EXPECT_DOUBLE_EQ(v, 0.0);
        }
    }
}