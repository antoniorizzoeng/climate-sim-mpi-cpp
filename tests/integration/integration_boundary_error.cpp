#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration_Main, BoundaryConditions_AndErrorHandling) {
    fs::remove_all("outputs");
    fs::create_directory("outputs");

    std::ostringstream cmd_ok;
    cmd_ok << MPIEXEC_EXECUTABLE << " " << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG << " "
           << INTEGRATION_MPI_PROCS << " " << CLIMATE_SIM_EXE << " --nx=16 --ny=16 --dx=1 --dy=1"
           << " --D=0 --vx=0 --vy=0"
           << " --dt=0.1 --steps=1 --out_every=1"
           << " --bc=periodic"
           << " --ic.mode=preset --ic.preset=gaussian_hotspot";
    ASSERT_EQ(run_cmd(cmd_ok.str()), 0);

    fs::remove_all("outputs");
    fs::create_directory("outputs");

    std::ostringstream cmd_bad_ic;
    cmd_bad_ic << MPIEXEC_EXECUTABLE << " " << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG
               << " " << INTEGRATION_MPI_PROCS << " " << CLIMATE_SIM_EXE
               << " --nx=16 --ny=16 --dx=1 --dy=1"
               << " --D=0 --vx=0 --vy=0"
               << " --dt=0.1 --steps=1 --out_every=1"
               << " --bc=periodic"
               << " --ic.mode=file --ic.format=bin --ic.path=inputs/does_not_exist.bin";
    const int ret = run_cmd(cmd_bad_ic.str());
    EXPECT_NE(ret, 0);

    fs::path bad_snapshot = "outputs/snapshots/snapshot_00000_rank00000.csv";
    ASSERT_FALSE(fs::exists(bad_snapshot));
}
