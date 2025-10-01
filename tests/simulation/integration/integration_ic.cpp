#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration, IC_LoadsCorrectMinMax) {
    fs::remove_all("outputs");

    std::vector<std::string> args = {"--nx=64",
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
                                     "--ic.preset=gaussian_hotspot",
                                     "--ic.A=1.0",
                                     "--ic.sigma_frac=0.1"};
    ASSERT_EQ(run_mpi_cmd(CLIMATE_SIM_EXE, args), 0);

    fs::path simfile = "outputs/snapshots.nc";
    ASSERT_TRUE(fs::exists(simfile));

    auto full = read_nc_2d(simfile, 0, "u");
    ASSERT_EQ(full.size(), 32u);
    ASSERT_EQ(full[0].size(), 64u);

    double mx_snap = -1e300;
    for (const auto& r : full)
        for (double v : r) mx_snap = std::max(mx_snap, v);
    EXPECT_GT(mx_snap, 1e-6);
}
