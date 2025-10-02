// integration_test.cpp
#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration, NetCDFOutput_WritesAndIsReadable) {
    fs::remove_all("outputs");

    std::vector<std::string> args = {"--nx=32",
                                     "--ny=32",
                                     "--dx=1",
                                     "--dy=1",
                                     "--D=0",
                                     "--vx=0",
                                     "--vy=0",
                                     "--dt=0.1",
                                     "--steps=1",
                                     "--out_every=1",
                                     "--bc=periodic",
                                     "--ic.mode=preset",
                                     "--ic.preset=gaussian_hotspot"};
    ASSERT_EQ(run_mpi_cmd(CLIMATE_SIM_EXE, args), 0);

    fs::path nc = "outputs/snapshots.nc";
    ASSERT_TRUE(fs::exists(nc)) << "NetCDF snapshot missing";

    auto grid = read_nc_2d(nc, 0, "u");
    ASSERT_EQ(grid.size(), 32u);
    ASSERT_EQ(grid[0].size(), 32u);
    EXPECT_GT(sum2d(grid), 0.0);
}
