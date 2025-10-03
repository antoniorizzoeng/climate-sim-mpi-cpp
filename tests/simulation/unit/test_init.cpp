#include <gtest/gtest.h>
#include <mpi.h>
#include <pnetcdf.h>

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

    cfg.bc.left = BCType::Periodic;
    cfg.bc.right = BCType::Periodic;
    cfg.bc.bottom = BCType::Periodic;
    cfg.bc.top = BCType::Periodic;

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
    for (int j = 0; j < u.ny_total(); ++j) {
        for (int i = 0; i < u.nx_total(); ++i) {
            if (u.at(i, j) > 1e-12)
                has_nonzero = true;
        }
    }
    EXPECT_TRUE(has_nonzero);
}

TEST(Unit_Init_IC, FileMissingThrows) {
    auto dec = make_decomp(2, 2, 2, 2);
    Field u(2, 2, 1, 1.0, 1.0);

    SimConfig cfg{};
    cfg.nx = cfg.ny = 2;
    cfg.dx = cfg.dy = 1;
    cfg.dt = 1;
    cfg.steps = 1;
    cfg.out_every = 1;
    cfg.ic.mode = "file";
    cfg.ic.path = "definitely_missing.nc";

    EXPECT_THROW(apply_initial_condition(dec, u, cfg), std::runtime_error);
}
