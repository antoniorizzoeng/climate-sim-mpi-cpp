#pragma once
#include <cmath>
#include <limits>

inline double safe_dt(double dx, double dy, double vx, double vy, double D) {
    const double denom_adv =
        (std::abs(vx) > 0 ? std::abs(vx)/dx : 0.0) +
        (std::abs(vy) > 0 ? std::abs(vy)/dy : 0.0);
    const double dt_adv = (denom_adv > 0)
        ? (1.0 / denom_adv)
        : std::numeric_limits<double>::infinity();

    const double denom_diff = (1.0/(dx*dx)) + (1.0/(dy*dy));
    const double dt_diff = (D > 0)
        ? (1.0 / (2.0 * D * denom_diff))
        : std::numeric_limits<double>::infinity();

    return std::min(dt_adv, dt_diff);
}
