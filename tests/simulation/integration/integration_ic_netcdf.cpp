#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration_Main, NetCDF_IC_Equivalence_WithBinaryIC) {
    fs::remove_all("inputs");
    fs::create_directories("inputs");
    fs::remove_all("outputs");
    fs::create_directories("outputs");

    {
        std::ostringstream py_nc;
        py_nc << PYTHON_EXECUTABLE << " " << (fs::path(SCRIPTS_DIR) / "generate_ic.py").string()
              << " --nx 40 --ny 24 --amp 1.0 --sigma-frac 0.12 --fmt netcdf --var T --out "
                 "inputs/ic_global";
        ASSERT_EQ(run_cmd(py_nc.str()), 0);

        std::ostringstream py_bin;
        py_bin << PYTHON_EXECUTABLE << " " << (fs::path(SCRIPTS_DIR) / "generate_ic.py").string()
               << " --nx 40 --ny 24 --amp 1.0 --sigma-frac 0.12 --fmt bin --out inputs/ic_global";
        ASSERT_EQ(run_cmd(py_bin.str()), 0);
    }

    {
        std::ostringstream cmd_nc;
        cmd_nc << MPIEXEC_EXECUTABLE << " " << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG
               << " " << INTEGRATION_MPI_PROCS << " " << CLIMATE_SIM_EXE
               << " --nx=40 --ny=24 --dx=1 --dy=1"
               << " --D=0 --vx=0 --vy=0"
               << " --dt=0.1 --steps=1 --out_every=1"
               << " --bc=periodic"
               << " --ic.mode=file --ic.format=netcdf --ic.path=inputs/ic_global.nc --ic.var=u"
               << " --output.format=netcdf";
        ASSERT_EQ(run_cmd(cmd_nc.str()), 0);

        fs::create_directories("outputs/snap_nc");
        fs::rename("outputs/snapshots", "outputs/snap_nc");
        fs::create_directories("outputs/snapshots");
    }

    {
        std::ostringstream cmd_bin;
        cmd_bin << MPIEXEC_EXECUTABLE << " " << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG
                << " " << INTEGRATION_MPI_PROCS << " " << CLIMATE_SIM_EXE
                << " --nx=40 --ny=24 --dx=1 --dy=1"
                << " --D=0 --vx=0 --vy=0"
                << " --dt=0.1 --steps=1 --out_every=1"
                << " --bc=periodic"
                << " --ic.mode=file --ic.format=bin --ic.path=inputs/ic_global.bin"
                << " --output.format=netcdf";
        ASSERT_EQ(run_cmd(cmd_bin.str()), 0);

        fs::create_directories("outputs/snap_bin");
        fs::rename("outputs/snapshots", "outputs/snap_bin");
    }

    auto U_nc = assemble_global_nc_snapshot_from("outputs/snap_nc", 0, "u");
    auto U_bin = assemble_global_nc_snapshot_from("outputs/snap_bin", 0, "u");

    ASSERT_EQ(U_nc.size(), U_bin.size());
    ASSERT_EQ(U_nc[0].size(), U_bin[0].size());
    for (size_t j = 0; j < U_nc.size(); ++j)
        for (size_t i = 0; i < U_nc[j].size(); ++i) ASSERT_NEAR(U_nc[j][i], U_bin[j][i], 1e-12);
}
