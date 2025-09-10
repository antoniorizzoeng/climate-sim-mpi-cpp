#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <vector>

#include "decomp.hpp"
#include "field.hpp"
#include "init.hpp"
#include "io.hpp"

#ifdef HAS_NETCDF
#include <netcdf.h>
#endif

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
    EXPECT_EQ(bc_to_string(cfg.bc), "periodic");
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
        "--nx=128", "--ny=256", "--dt=0.2", "--bc=periodic", "--output_prefix=from_cli"};

    SimConfig merged = merged_config(tmpfile, args);

    EXPECT_EQ(merged.nx, 128);                       // CLI wins
    EXPECT_EQ(merged.ny, 256);                       // CLI wins
    EXPECT_DOUBLE_EQ(merged.dt, 0.2);                // CLI wins
    EXPECT_EQ(bc_to_string(merged.bc), "periodic");  // CLI wins
    EXPECT_EQ(merged.output_prefix, "from_cli");     // CLI wins
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
}

TEST(Unit_IO_CLI, InvalidBoundaryConditionThrows) {
    std::vector<std::string> args = {"--bc=foobar"};
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

TEST(Unit_IO_File, WriteAndReadCSV) {
    Field f(2, 2, 0, 1.0, 1.0);
    f.at(0, 0) = 1.1;
    f.at(1, 0) = 2.2;
    f.at(0, 1) = 3.3;
    f.at(1, 1) = 4.4;

    const std::string fname = "field.csv";
    write_field_csv(f, fname);

    std::ifstream ifs(fname);
    std::string line;
    std::getline(ifs, line);
    EXPECT_NE(line.find("1.1"), std::string::npos);
    std::remove(fname.c_str());
}

TEST(Unit_IO_File, SnapshotFilenames) {
    auto csv = snapshot_filename("outdir", 7, 3);
    EXPECT_NE(csv.find("snapshot_00007_rank00003.csv"), std::string::npos);
    auto nc = snapshot_filename_nc("outdir", 7, 3);
    EXPECT_NE(nc.find("snapshot_00007_rank00003.nc"), std::string::npos);
}

TEST(Unit_IO_File, RankLayoutAppendAndHeader) {
    const std::string fname = "layout.csv";
    write_rank_layout_csv(fname, 0, 8, 8, 0, 0, 8, 8, 1);
    write_rank_layout_csv(fname, 1, 8, 8, 0, 0, 8, 8, 1);

    std::ifstream ifs(fname);
    std::string all((std::istreambuf_iterator<char>(ifs)), {});
    EXPECT_NE(all.find("rank,x_offset"), std::string::npos);
    EXPECT_NE(all.find("1,"), std::string::npos);
    std::remove(fname.c_str());
}

#ifdef HAS_NETCDF
TEST(Unit_IO_File, WriteNetCDFAndReadBack) {
    auto dec = make_decomp(2, 2, 2, 2);
    Field f(2, 2, 0, 1.0, 1.0);
    f.at(0, 0) = 1.0;
    f.at(1, 0) = 2.0;
    f.at(0, 1) = 3.0;
    f.at(1, 1) = 4.0;

    const std::string fname = "field.nc";
    ASSERT_TRUE(write_field_netcdf(f, fname, dec));

    int ncid, varid;
    ASSERT_EQ(nc_open(fname.c_str(), NC_NOWRITE, &ncid), NC_NOERR);
    ASSERT_EQ(nc_inq_varid(ncid, "u", &varid), NC_NOERR);
    std::vector<double> buf(4);
    ASSERT_EQ(nc_get_var_double(ncid, varid, buf.data()), NC_NOERR);
    EXPECT_NEAR(buf[3], 4.0, 1e-12);
    nc_close(ncid);

    std::remove(fname.c_str());
}
#endif