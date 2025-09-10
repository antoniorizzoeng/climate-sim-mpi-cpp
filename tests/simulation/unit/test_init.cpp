#include <gtest/gtest.h>
#include <netcdf.h>

#include <fstream>

#include "field.hpp"
#include "init.hpp"
#include "io.hpp"

static SimConfig base_config(int nx = 16, int ny = 12) {
    SimConfig cfg{};
    cfg.nx = nx;
    cfg.ny = ny;
    cfg.dx = 1.0;
    cfg.dy = 1.0;
    cfg.dt = 0.1;
    cfg.steps = 1;
    cfg.out_every = 1;
    cfg.bc = BCType::Periodic;
    cfg.ic.mode = "preset";
    cfg.ic.preset = "gaussian_hotspot";
    cfg.ic.A = 1.0;
    cfg.ic.sigma_frac = 0.1;
    cfg.ic.xc_frac = 0.5;
    cfg.ic.yc_frac = 0.5;
    return cfg;
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

TEST(Unit_Init_IC, ConstantZeroPreset) {
    auto dec = make_decomp(16, 16, 16, 16);
    Field u(dec.nx_local, dec.ny_local, 1, 1.0, 1.0);
    u.fill(0.0);

    SimConfig cfg{};
    cfg.ic.mode = "preset";
    cfg.ic.preset = "constant_zero";

    apply_initial_condition(dec, u, cfg);

    for (int j = 0; j < u.ny_total(); ++j)
        for (int i = 0; i < u.nx_total(); ++i) EXPECT_DOUBLE_EQ(u.at(i, j), 0.0);
}

TEST(Unit_Init_IC, GaussianHotspotPreset) {
    auto dec = make_decomp(32, 32, 32, 32);
    Field u(dec.nx_local, dec.ny_local, 1, 1.0, 1.0);
    u.fill(0.0);

    SimConfig cfg = base_config(32, 32);

    apply_initial_condition(dec, u, cfg);

    bool has_nonzero = false;
    for (int j = 0; j < u.ny_total(); ++j)
        for (int i = 0; i < u.nx_total(); ++i)
            if (u.at(i, j) > 1e-12)
                has_nonzero = true;

    EXPECT_TRUE(has_nonzero);
}

#ifdef HAS_NETCDF
TEST(Unit_Init, NetCDFIC_SucceedsAndMissingVarThrows) {
    auto dec = make_decomp(8, 6, 8, 6);
    Field u(dec.nx_local, dec.ny_local, 1, 1.0, 1.0);

    const std::string nc_ok = "tmp_ic_ok.nc";
    const std::string var = "u";

    {
        int ncid, dimy, dimx, varid;
        ASSERT_EQ(nc_create(nc_ok.c_str(), NC_CLOBBER, &ncid), NC_NOERR);
        ASSERT_EQ(nc_def_dim(ncid, "y", dec.ny_global, &dimy), NC_NOERR);
        ASSERT_EQ(nc_def_dim(ncid, "x", dec.nx_global, &dimx), NC_NOERR);
        int dims[2] = {dimy, dimx};
        ASSERT_EQ(nc_def_var(ncid, var.c_str(), NC_DOUBLE, 2, dims, &varid), NC_NOERR);
        ASSERT_EQ(nc_enddef(ncid), NC_NOERR);

        std::vector<double> data(dec.nx_global * dec.ny_global, 0.25);
        ASSERT_EQ(nc_put_var_double(ncid, varid, data.data()), NC_NOERR);
        ASSERT_EQ(nc_close(ncid), NC_NOERR);
    }

    SimConfig cfg{};
    cfg.ic.mode = "file";
    cfg.ic.format = "netcdf";
    cfg.ic.path = nc_ok;
    cfg.ic.var = var;

    apply_initial_condition(dec, u, cfg);
    EXPECT_NEAR(u.at(1, 1), 0.25, 1e-12);

    cfg.ic.var = "does_not_exist";
    EXPECT_THROW(apply_initial_condition(dec, u, cfg), std::runtime_error);

    remove(nc_ok.c_str());
}

TEST(Unit_Init_IC, NetCDFNotBuiltThrows) {
    auto dec = make_decomp(2, 2, 2, 2);
    Field u(2, 2, 1, 1.0, 1.0);

    SimConfig cfg{};
    cfg.nx = cfg.ny = 2;
    cfg.dx = cfg.dy = 1;
    cfg.dt = 1;
    cfg.steps = 1;
    cfg.out_every = 1;
    cfg.ic.mode = "file";
    cfg.ic.format = "netcdf";
    cfg.ic.path = "dummy.nc";

    EXPECT_THROW(apply_initial_condition(dec, u, cfg), std::runtime_error);
}
#endif

TEST(Unit_Init_IC, UnsupportedFormatThrows) {
    auto dec = make_decomp(4, 4, 4, 4);
    Field u(4, 4, 1, 1.0, 1.0);

    SimConfig cfg{};
    cfg.nx = cfg.ny = 4;
    cfg.dx = cfg.dy = 1;
    cfg.dt = 1;
    cfg.steps = 1;
    cfg.out_every = 1;
    cfg.ic.mode = "file";
    cfg.ic.format = "foobar";
    cfg.ic.path = "nonexistent";

    EXPECT_THROW(apply_initial_condition(dec, u, cfg), std::runtime_error);
}

TEST(Unit_Init_IC, BinaryFileMissingThrows) {
    auto dec = make_decomp(2, 2, 2, 2);
    Field u(2, 2, 1, 1.0, 1.0);

    SimConfig cfg{};
    cfg.nx = cfg.ny = 2;
    cfg.dx = cfg.dy = 1;
    cfg.dt = 1;
    cfg.steps = 1;
    cfg.out_every = 1;
    cfg.ic.mode = "file";
    cfg.ic.format = "bin";
    cfg.ic.path = "definitely_missing.bin";

    EXPECT_THROW(apply_initial_condition(dec, u, cfg), std::runtime_error);
}

TEST(Unit_Init_IC, BinaryFileTruncatedThrows) {
    auto dec = make_decomp(2, 2, 2, 2);
    Field u(2, 2, 1, 1.0, 1.0);

    const std::string fname = "truncated.bin";
    {
        std::ofstream ofs(fname, std::ios::binary);
        double val = 1.23;
        ofs.write(reinterpret_cast<char*>(&val), sizeof(double));
    }

    SimConfig cfg{};
    cfg.nx = cfg.ny = 2;
    cfg.dx = cfg.dy = 1;
    cfg.dt = 1;
    cfg.steps = 1;
    cfg.out_every = 1;
    cfg.ic.mode = "file";
    cfg.ic.format = "bin";
    cfg.ic.path = fname;

    EXPECT_THROW(apply_initial_condition(dec, u, cfg), std::runtime_error);

    std::remove(fname.c_str());
}