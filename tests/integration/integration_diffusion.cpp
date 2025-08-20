#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration, Diffusion_DecreasesPeak) {
    fs::remove_all("outputs");
    fs::create_directories("outputs/snapshots");

    std::ostringstream cmd;
    cmd << MPIEXEC_EXECUTABLE << " " << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG << " "
        << INTEGRATION_MPI_PROCS << " " << CLIMATE_SIM_EXE << " --nx=64 --ny=64 --dx=1 --dy=1"
        << " --D=1.0 --vx=0 --vy=0"
        << " --dt=0.1 --steps=11 --out_every=10"
        << " --bc=periodic"
        << " --ic.mode=preset --ic.preset=gaussian_hotspot --ic.A=1.0 --ic.sigma_frac=0.1";
    ASSERT_EQ(run_cmd(cmd.str()), 0);

    fs::path snap0_r0 = fs::path("outputs/snapshots") / "snapshot_00000_rank00000.csv";
    fs::path snap10_r0 = fs::path("outputs/snapshots") / "snapshot_00010_rank00000.csv";
    ASSERT_TRUE(fs::exists(snap0_r0));
    ASSERT_TRUE(fs::exists(snap10_r0));

    auto U0 = assemble_global_csv_snapshot(0);
    auto U10 = assemble_global_csv_snapshot(10);
    ASSERT_FALSE(U0.empty());
    ASSERT_FALSE(U10.empty());
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