#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration, Advection_ShiftsHotspotRight_NC) {
    fs::remove_all("outputs");

    std::vector<std::string> args = {"--nx=64",
                                     "--ny=64",
                                     "--dx=1",
                                     "--dy=1",
                                     "--D=0",
                                     "--vx=1",
                                     "--vy=0",
                                     "--dt=1",
                                     "--steps=6",
                                     "--out_every=1",
                                     "--bc=periodic",
                                     "--ic.mode=preset",
                                     "--ic.preset=gaussian_hotspot",
                                     "--ic.sigma_frac=0.1",
                                     "--ic.A=1.0"};
    ASSERT_EQ(run_mpi_cmd(CLIMATE_SIM_EXE, args), 0);

    fs::path simfile = "outputs/snapshots.nc";
    ASSERT_TRUE(fs::exists(simfile));

    auto U0 = read_nc_2d(simfile, 0, "u");
    auto U5 = read_nc_2d(simfile, 5, "u");

    double x0 = com_x(U0);
    double x5 = com_x(U5);
    EXPECT_NEAR((x5 - x0), 5.0, 1.0);

    EXPECT_NEAR(sum2d(U5), sum2d(U0), 0.05 * sum2d(U0));
}
