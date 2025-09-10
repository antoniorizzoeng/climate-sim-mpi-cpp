#include <gtest/gtest.h>

#include "advection.hpp"
#include "field.hpp"

static Field make_hotspot(int nx, int ny, int halo = 1) {
    Field f(nx, ny, halo, 1.0, 1.0);
    f.fill(0.0);
    f.at(nx / 2 + halo, ny / 2 + halo) = 1.0;
    return f;
}

TEST(Unit_Advection, ZeroVelocityNoChange) {
    int nx = 8, ny = 8;
    Field u = make_hotspot(nx, ny);
    Field out(nx, ny, 1, 1.0, 1.0);
    out.fill(0.0);

    advection_step(u, out, 0.0, 0.0, 0.1);

    for (int j = 1; j <= ny; ++j)
        for (int i = 1; i <= nx; ++i) EXPECT_DOUBLE_EQ(out.at(i, j), 0.0);
}

TEST(Unit_Advection, PositiveVxAffectsNeighbor) {
    int nx = 8, ny = 8;
    Field u = make_hotspot(nx, ny);
    Field out(nx, ny, 1, 1.0, 1.0);
    out.fill(0.0);

    advection_step(u, out, +1.0, 0.0, 0.1);

    int cx = nx / 2 + 1, cy = ny / 2 + 1;
    EXPECT_NE(out.at(cx, cy), 0.0);
}

TEST(Unit_Advection, NegativeVxAffectsNeighbor) {
    int nx = 8, ny = 8;
    Field u = make_hotspot(nx, ny);
    Field out(nx, ny, 1, 1.0, 1.0);
    out.fill(0.0);

    advection_step(u, out, -1.0, 0.0, 0.1);

    int cx = nx / 2 + 1, cy = ny / 2 + 1;
    EXPECT_NE(out.at(cx, cy), 0.0);
}

TEST(Unit_Advection, PositiveVyAffectsNeighbor) {
    int nx = 8, ny = 8;
    Field u = make_hotspot(nx, ny);
    Field out(nx, ny, 1, 1.0, 1.0);
    out.fill(0.0);

    advection_step(u, out, 0.0, +1.0, 0.1);

    int cx = nx / 2 + 1, cy = ny / 2 + 1;
    EXPECT_NE(out.at(cx, cy), 0.0);
}

TEST(Unit_Advection, NegativeVyAffectsNeighbor) {
    int nx = 8, ny = 8;
    Field u = make_hotspot(nx, ny);
    Field out(nx, ny, 1, 1.0, 1.0);
    out.fill(0.0);

    advection_step(u, out, 0.0, -1.0, 0.1);

    int cx = nx / 2 + 1, cy = ny / 2 + 1;
    EXPECT_NE(out.at(cx, cy), 0.0);
}
