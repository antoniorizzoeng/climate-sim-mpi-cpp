#ifdef HAS_NETCDF
#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration, CSV_vs_NetCDF_Equivalence_Global) {
    fs::remove_all("outputs");
    fs::create_directories("outputs/snapshots");

    std::ostringstream cmd_csv;
    cmd_csv << MPIEXEC_EXECUTABLE << " " << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG << " "
            << INTEGRATION_MPI_PROCS << " " << CLIMATE_SIM_EXE << " --nx=40 --ny=24 --dx=1 --dy=1"
            << " --D=0.0 --vx=0 --vy=0"
            << " --dt=0.1 --steps=1 --out_every=1"
            << " --bc=periodic"
            << " --ic.mode=preset --ic.preset=gaussian_hotspot --ic.A=1.0 --ic.sigma_frac=0.12"
            << " --output.format=csv";
    ASSERT_EQ(run_cmd(cmd_csv.str()), 0);

    fs::create_directories("outputs/snap_csv");
    fs::rename("outputs/snapshots", "outputs/snap_csv");
    fs::create_directories("outputs/snapshots");

    std::ostringstream cmd_nc;
    cmd_nc << MPIEXEC_EXECUTABLE << " " << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG << " "
           << INTEGRATION_MPI_PROCS << " " << CLIMATE_SIM_EXE << " --nx=40 --ny=24 --dx=1 --dy=1"
           << " --D=0.0 --vx=0 --vy=0"
           << " --dt=0.1 --steps=1 --out_every=1"
           << " --bc=periodic"
           << " --ic.mode=preset --ic.preset=gaussian_hotspot --ic.A=1.0 --ic.sigma_frac=0.12"
           << " --output.format=netcdf";
    ASSERT_EQ(run_cmd(cmd_nc.str()), 0);

    auto Ucsv = assemble_global_csv_snapshot_from("outputs/snap_csv", 0);
    auto Unc = assemble_global_nc_snapshot_from("outputs/snapshots", 0, "u");

    ASSERT_EQ(Ucsv.size(), Unc.size());
    ASSERT_EQ(Ucsv[0].size(), Unc[0].size());

    for (size_t j = 0; j < Ucsv.size(); ++j) {
        for (size_t i = 0; i < Ucsv[0].size(); ++i) {
            double diff = std::abs(Ucsv[j][i] - Unc[j][i]);
            ASSERT_LE(diff, 1e-12) << "mismatch at (" << i << "," << j << ")";
        }
    }
}
#endif