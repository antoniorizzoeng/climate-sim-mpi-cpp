#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration, Advection_ShiftsHotspotRight_NC) {
    fs::remove_all("outputs");

    std::ostringstream cmd;
    cmd << MPIEXEC_EXECUTABLE << " " << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG << " "
        << INTEGRATION_MPI_PROCS << " " << CLIMATE_SIM_EXE << " --nx=64 --ny=64 --dx=1 --dy=1"
        << " --D=0 --vx=1 --vy=0"
        << " --dt=1 --steps=6 --out_every=1"
        << " --bc=periodic"
        << " --ic.mode=preset --ic.preset=gaussian_hotspot"
        << " --ic.sigma_frac=0.1 --ic.A=1.0";
    ASSERT_EQ(run_cmd(cmd.str()), 0);

    auto U0 = assemble_global_nc_snapshot_from("outputs/snapshots", 0, "u");
    auto U5 = assemble_global_nc_snapshot_from("outputs/snapshots", 5, "u");

    double x0 = com_x(U0);
    double x5 = com_x(U5);
    EXPECT_NEAR((x5 - x0), 5.0, 1.0);

    double s0 = sum2d(U0);
    double s5 = sum2d(U5);
    EXPECT_NEAR(s5, s0, 0.05 * s0);
}
