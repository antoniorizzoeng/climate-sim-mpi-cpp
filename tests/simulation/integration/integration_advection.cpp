#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration, Advection_ShiftsHotspotRight_NC) {
    const TempRunDirectory dir;
    const auto snapshot = dir.path / "new/nested/snapshots.nc";

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
    args.push_back("--output=" + snapshot.string());
    ASSERT_EQ(run_mpi_cmd(CLIMATE_SIM_EXE, args), 0);

    ASSERT_TRUE(fs::exists(snapshot));

    auto U0 = read_nc_2d(snapshot, 0, "u");
    auto U5 = read_nc_2d(snapshot, 5, "u");

    double x0 = com_x(U0);
    double x5 = com_x(U5);
    EXPECT_NEAR((x5 - x0), 5.0, 1.0);

    EXPECT_NEAR(sum2d(U5), sum2d(U0), 0.05 * sum2d(U0));
}
