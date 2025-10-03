#include <gtest/gtest.h>
#include <mpi.h>
#include <pnetcdf.h>

#include <fstream>
#include <string>
#include <vector>

#include "decomp.hpp"
#include "field.hpp"
#include "init.hpp"
#include "io.hpp"

static std::string cfg_path(const char* fname) {
#ifdef CONFIGS_DIR
    return std::string(CONFIGS_DIR) + "/" + fname;
#else
    return fname;
#endif
}

static Decomp2D make_decomp(
    int nx_global, int ny_global, int nx_local, int ny_local, int x_off = 0, int y_off = 0) {
    Decomp2D d{};
    d.nx_global = nx_global;
    d.ny_global = ny_global;
    d.nx_local = nx_local;
    d.ny_local = ny_local;
    d.x_offset = x_off;
    d.y_offset = y_off;
    return d;
}

TEST(Unit_IO_Yaml, LoadsNestedBlocksAndBC) {
    SimConfig cfg = load_yaml_file(cfg_path("dev.yaml"));
    EXPECT_GT(cfg.nx, 0);
    EXPECT_GT(cfg.ny, 0);
    EXPECT_GT(cfg.dt, 0.0);

    EXPECT_EQ(bc_to_string(cfg.bc.left), "dirichlet");
    EXPECT_EQ(bc_to_string(cfg.bc.right), "neumann");
    EXPECT_EQ(bc_to_string(cfg.bc.bottom), "periodic");
    EXPECT_EQ(bc_to_string(cfg.bc.top), "dirichlet");

    EXPECT_GE(cfg.D, 0.0);
}

TEST(Unit_IO_CLI, SimpleScalarOverrides) {
    auto tmpfile = "tmp_test.yaml";
    {
        std::ofstream ofs(tmpfile);
        ofs << "grid: { nx: 64, ny: 64, dx: 1.0, dy: 1.0 }\n"
            << "physics: { D: 0.01, vx: 0.0, vy: 0.0 }\n"
            << "time: { dt: 0.1, steps: 10, out_every: 5 }\n"
            << "bc: dirichlet\n"
            << "output: { prefix: \"from_yaml\" }\n";
    }

    std::vector<std::string> args = {
        "--nx=128", "--ny=256", "--dt=0.2", "--bc.left=periodic", "--output_prefix=from_cli"};

    SimConfig merged = merged_config(tmpfile, args);

    EXPECT_EQ(merged.nx, 128);
    EXPECT_EQ(merged.ny, 256);
    EXPECT_DOUBLE_EQ(merged.dt, 0.2);
    EXPECT_EQ(bc_to_string(merged.bc.left), "periodic");
    EXPECT_EQ(bc_to_string(merged.bc.right), "dirichlet");
    EXPECT_EQ(merged.output_prefix, "from_cli");
}

TEST(Unit_IO_CLI, ICOverridesTakePrecedence) {
    std::vector<std::string> args = {"--ic.mode=preset",
                                     "--ic.preset=constant_zero",
                                     "--ic.A=999.0",
                                     "--ic.sigma_frac=0.25",
                                     "--ic.xc_frac=0.1",
                                     "--ic.yc_frac=0.2"};
    SimConfig merged = merged_config(cfg_path("dev.yaml"), args);

    EXPECT_EQ(merged.ic.mode, "preset");
    EXPECT_EQ(merged.ic.preset, "constant_zero");
    EXPECT_DOUBLE_EQ(merged.ic.A, 999.0);
    EXPECT_DOUBLE_EQ(merged.ic.sigma_frac, 0.25);
    EXPECT_DOUBLE_EQ(merged.ic.xc_frac, 0.1);
    EXPECT_DOUBLE_EQ(merged.ic.yc_frac, 0.2);
}

TEST(Unit_IO_BC, ParseRoundtrip) {
    EXPECT_EQ(bc_from_string("dirichlet"), BCType::Dirichlet);
    EXPECT_EQ(bc_from_string("neumann"), BCType::Neumann);
    EXPECT_EQ(bc_from_string("periodic"), BCType::Periodic);

    EXPECT_EQ(bc_to_string(BCType::Dirichlet), std::string("dirichlet"));
    EXPECT_EQ(bc_to_string(BCType::Neumann), std::string("neumann"));
    EXPECT_EQ(bc_to_string(BCType::Periodic), std::string("periodic"));

    BCConfig bc;
    bc.left = BCType::Neumann;
    bc.right = BCType::Dirichlet;
    bc.bottom = BCType::Periodic;
    bc.top = BCType::Neumann;
    EXPECT_EQ(bc_to_string(bc.left), "neumann");
    EXPECT_EQ(bc_to_string(bc.right), "dirichlet");
    EXPECT_EQ(bc_to_string(bc.bottom), "periodic");
    EXPECT_EQ(bc_to_string(bc.top), "neumann");
}

TEST(Unit_IO_CLI, InvalidBoundaryConditionThrows) {
    std::vector<std::string> args = {"--bc.left=foobar"};
    EXPECT_THROW({ merged_config(std::nullopt, args); }, std::runtime_error);
}

TEST(Unit_IO_CLI, InvalidGridSizeThrows) {
    std::vector<std::string> args = {"--nx=-10", "--ny=128"};
    EXPECT_THROW({ merged_config(std::nullopt, args); }, std::runtime_error);
}

TEST(Unit_IO_CLI, InvalidTimestepThrows) {
    std::vector<std::string> args = {"--dt=0.0", "--steps=10"};
    EXPECT_THROW({ merged_config(std::nullopt, args); }, std::runtime_error);
}

TEST(Unit_IO_CLI, InvalidICPresetThrows) {
    SimConfig cfg;
    cfg.ic.mode = "preset";
    cfg.ic.preset = "notarealpreset";

    Field f(cfg.nx, cfg.ny, 0, cfg.dx, cfg.dy);
    Decomp2D dec;
    EXPECT_THROW({ apply_initial_condition(dec, f, cfg); }, std::runtime_error);
}

TEST(Unit_IO_BC, BcToStringDefaultCase) {
    auto bogus = static_cast<BCType>(999);
    EXPECT_EQ(bc_to_string(bogus), "dirichlet");
}

TEST(Unit_IO_Yaml, MissingBlocksStillWork) {
    const std::string fname = "minimal.yaml";
    {
        std::ofstream ofs(fname);
        ofs << "nx: 4\nny: 5\ndx: 1.0\ndy: 1.0\n"
            << "dt: 0.1\nsteps: 2\nout_every: 1\n";
    }

    SimConfig cfg = load_yaml_file(fname);
    EXPECT_EQ(cfg.nx, 4);
    EXPECT_EQ(cfg.ny, 5);
    std::remove(fname.c_str());
}

TEST(Unit_IO_CLI, OverridesWithSpaceSeparator) {
    std::vector<std::string> args = {"--nx", "42", "--dy", "2.5", "--output.prefix", "cli_space"};
    SimConfig merged = merged_config(std::nullopt, args);
    EXPECT_EQ(merged.nx, 42);
    EXPECT_DOUBLE_EQ(merged.dy, 2.5);
    EXPECT_EQ(merged.output_prefix, "cli_space");
}

TEST(Unit_IO_CLI, MergedConfigNoYaml) {
    std::vector<std::string> args = {"--nx=8", "--ny=8", "--dt=0.1", "--steps=1"};
    SimConfig cfg = merged_config(std::nullopt, args);
    EXPECT_EQ(cfg.nx, 8);
    EXPECT_EQ(cfg.ny, 8);
}

TEST(Unit_IO_File, WriteNetCDFAndReadBack) {
    int argc = 0;
    char** argv = nullptr;
    MPI_Init(&argc, &argv);
    {
        auto dec = make_decomp(2, 2, 2, 2);
        Field f(2, 2, 0, 1.0, 1.0);
        f.at(0, 0) = 1.0;
        f.at(1, 0) = 2.0;
        f.at(0, 1) = 3.0;
        f.at(1, 1) = 4.0;

        const std::string fname = "field.nc";
        int ncid, dimy, dimx, dimt, varid;
        ASSERT_EQ(ncmpi_create(MPI_COMM_WORLD, fname.c_str(), NC_CLOBBER, MPI_INFO_NULL, &ncid),
                  NC_NOERR);
        ASSERT_EQ(ncmpi_def_dim(ncid, "time", NC_UNLIMITED, &dimt), NC_NOERR);
        ASSERT_EQ(ncmpi_def_dim(ncid, "y", dec.ny_global, &dimy), NC_NOERR);
        ASSERT_EQ(ncmpi_def_dim(ncid, "x", dec.nx_global, &dimx), NC_NOERR);

        int dims[3] = {dimt, dimy, dimx};
        ASSERT_EQ(ncmpi_def_var(ncid, "u", NC_DOUBLE, 3, dims, &varid), NC_NOERR);
        ASSERT_EQ(ncmpi_enddef(ncid), NC_NOERR);

        ASSERT_TRUE(write_field_netcdf(ncid, 0, f, dec, varid));

        ASSERT_EQ(ncmpi_close(ncid), NC_NOERR);

        int ncid2, varid2;
        ASSERT_EQ(ncmpi_open(MPI_COMM_WORLD, fname.c_str(), NC_NOWRITE, MPI_INFO_NULL, &ncid2),
                  NC_NOERR);
        ASSERT_EQ(ncmpi_inq_varid(ncid2, "u", &varid2), NC_NOERR);
        std::vector<double> buf(4);
        ASSERT_EQ(ncmpi_get_var_double_all(ncid2, varid2, buf.data()), NC_NOERR);
        EXPECT_NEAR(buf[3], 4.0, 1e-12);
        ncmpi_close(ncid2);

        std::remove(fname.c_str());
    }
}
