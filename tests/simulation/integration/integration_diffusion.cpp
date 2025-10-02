#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration, Diffusion_DecreasesPeak) {
    fs::remove_all("outputs");

    std::vector<std::string> args = {"--nx=64",
                                     "--ny=64",
                                     "--dx=1",
                                     "--dy=1",
                                     "--D=1.0",
                                     "--vx=0",
                                     "--vy=0",
                                     "--dt=0.1",
                                     "--steps=10",
                                     "--out_every=1",
                                     "--bc=periodic",
                                     "--ic.mode=preset",
                                     "--ic.preset=gaussian_hotspot",
                                     "--ic.A=1.0",
                                     "--ic.sigma_frac=0.1"};
    ASSERT_EQ(run_mpi_cmd(CLIMATE_SIM_EXE, args), 0);

    fs::path simfile = "outputs/snapshots.nc";
    ASSERT_TRUE(fs::exists(simfile));

    auto U0 = read_nc_2d(simfile, 0, "u");
    auto U10 = read_nc_2d(simfile, 9, "u");

    ASSERT_EQ(U0.size(), 64u);
    ASSERT_EQ(U0[0].size(), 64u);
    ASSERT_EQ(U10.size(), 64u);
    ASSERT_EQ(U10[0].size(), 64u);

    auto peak = [](const auto& G) {
        double mx = -1e300;
        for (const auto& row : G)
            for (double v : row) mx = std::max(mx, v);
        return mx;
    };
    double mx0 = peak(U0);
    double mx10 = peak(U10);
    EXPECT_LT(mx10, mx0);

    for (const auto& row : U10)
        for (double v : row) EXPECT_GE(v, 0.0);
}
