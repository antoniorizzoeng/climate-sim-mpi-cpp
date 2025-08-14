#include <gtest/gtest.h>

#include "stability.hpp"

TEST(StabilityTest, ReturnsPositiveLimit) {
    double dx = 1.0, dy = 1.0;
    double vx = 0.5, vy = 0.5;
    double D = 0.1;
    double limit = safe_dt(dx, dy, vx, vy, D);
    EXPECT_GT(limit, 0.0);
}

TEST(StabilityTest, LowerWithHigherVelocity) {
    double dx = 1.0, dy = 1.0;
    double D = 0.1;
    double low_v = safe_dt(dx, dy, 0.5, 0.5, D);
    double high_v = safe_dt(dx, dy, 5.0, 5.0, D);
    EXPECT_LT(high_v, low_v);
}

TEST(StabilityTest, LowerWithHigherDiffusion) {
    double dx = 1.0, dy = 1.0;
    double vx = 0.5, vy = 0.5;
    double low_D = safe_dt(dx, dy, vx, vy, 0.1);
    double high_D = safe_dt(dx, dy, vx, vy, 1.0);
    EXPECT_LT(high_D, low_D);
}
