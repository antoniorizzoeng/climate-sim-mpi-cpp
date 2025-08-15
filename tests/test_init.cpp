#include <gtest/gtest.h>

#include <algorithm>

#include "decomp.hpp"
#include "field.hpp"
#include "init.hpp"
#include "io.hpp"

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

static std::pair<double, double> minmax(const Field& f) {
    double mn = 1e300, mx = -1e300;
    for (int j = 0; j < f.ny_total(); ++j)
        for (int i = 0; i < f.nx_total(); ++i) {
            double v = f.at(i, j);
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }
    return {mn, mx};
}

TEST(Init_IC, ConstantZeroPreset) {
    auto dec = make_decomp(16, 16, 16, 16);
    Field u(dec.nx_local, dec.ny_local, 1, 1.0, 1.0);
    u.fill(0.0);

    SimConfig cfg;
    cfg.ic.mode = "preset";
    cfg.ic.preset = "constant_zero";

    apply_initial_condition(dec, u, cfg);

    auto [mn, mx] = minmax(u);
    EXPECT_DOUBLE_EQ(mn, 0.0);
    EXPECT_DOUBLE_EQ(mx, 0.0);
}

TEST(Init_IC, GaussianHotspotPreset) {
    auto dec = make_decomp(64, 64, 64, 64);
    Field u(dec.nx_local, dec.ny_local, 1, 1.0, 1.0);
    u.fill(0.0);

    SimConfig cfg;
    cfg.ic.mode = "preset";
    cfg.ic.preset = "gaussian_hotspot";
    cfg.ic.A = 1.0;
    cfg.ic.sigma_frac = 0.10;
    cfg.ic.xc_frac = 0.50;
    cfg.ic.yc_frac = 0.50;

    apply_initial_condition(dec, u, cfg);

    auto [mn, mx] = minmax(u);
    EXPECT_GE(mn, 0.0);
    EXPECT_GT(mx, 1e-6);
}
