#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration, BinaryIC_LoadsCorrectMinMax) {
    fs::remove_all("inputs");
    fs::create_directories("inputs");
    std::ostringstream py;
    py << PYTHON_EXECUTABLE << " " << (fs::path(SCRIPTS_DIR) / "generate_ic.py").string()
       << " --nx 64 --ny 32 --amp 1.0 --sigma-frac 0.1 --fmt bin --out inputs/ic_global";
    ASSERT_EQ(run_cmd(py.str()), 0);

    auto plane = read_bin_plane("inputs/ic_global.bin", 64, 32);
    ASSERT_EQ(plane.size(), 64u * 32u);
    double mx = -1e300;
    for (double v : plane) mx = std::max(mx, v);
    ASSERT_GT(mx, 1e-6);

    fs::remove_all("outputs");
    std::ostringstream cmd;
    cmd << MPIEXEC_EXECUTABLE << " " << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG << " "
        << INTEGRATION_MPI_PROCS << " " << CLIMATE_SIM_EXE << " --nx=64 --ny=32 --dx=1 --dy=1"
        << " --D=0 --vx=0 --vy=0"
        << " --dt=0.1 --steps=1 --out_every=1"
        << " --bc=periodic"
        << " --ic.mode=file --ic.format=bin --ic.path=inputs/ic_global.bin";
    ASSERT_EQ(run_cmd(cmd.str()), 0);

    auto full = assemble_global_csv_snapshot(0);
    ASSERT_EQ(full.size(), 32u);
    ASSERT_EQ(full[0].size(), 64u);

    double mx_snap = -1e300;
    for (auto& r : full)
        for (double v : r) mx_snap = std::max(mx_snap, v);

    EXPECT_NEAR(mx_snap, mx, 1e-3);
}