#ifdef HAS_NETCDF
#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration, NetCDFOutput_WritesAndIsReadable) {
    fs::remove_all("outputs");

    std::ostringstream cmd;
    cmd << MPIEXEC_EXECUTABLE << " " << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG << " "
        << INTEGRATION_MPI_PROCS << " " << CLIMATE_SIM_EXE << " --nx=32 --ny=32 --dx=1 --dy=1"
        << " --D=0 --vx=0 --vy=0"
        << " --dt=0.1 --steps=1 --out_every=1"
        << " --bc=periodic"
        << " --ic.mode=preset --ic.preset=gaussian_hotspot"
        << " --output.format=netcdf";
    ASSERT_EQ(run_cmd(cmd.str()), 0);

    fs::path nc = fs::path("outputs/snapshots") / "snapshot_00000_rank00000.nc";
    ASSERT_TRUE(fs::exists(nc)) << "NetCDF snapshot missing";

    auto grid = read_nc_2d(nc, "u");
    ASSERT_FALSE(grid.empty());
    ASSERT_FALSE(grid[0].empty());

    double mn = 1e300, mx = -1e300;
    for (auto& r : grid)
        for (double v : r) {
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }
    EXPECT_GE(mn, 0.0);
    EXPECT_LE(mx, 1.0);
    EXPECT_GT(mx, 1e-6);
}
#endif