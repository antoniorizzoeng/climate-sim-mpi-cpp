#pragma once
#include <cmath>
#include <limits>

inline double safe_dt(double dx, double dy, double vx, double vy, double D) {
    const double denom_adv =
        (std::abs(vx) > 0 ? std::abs(vx) / dx : 0.0) + (std::abs(vy) > 0 ? std::abs(vy) / dy : 0.0);
    const double dt_adv =
        (denom_adv > 0) ? (1.0 / denom_adv) : std::numeric_limits<double>::infinity();

    const double denom_diff = (1.0 / (dx * dx)) + (1.0 / (dy * dy));
    const double dt_diff =
        (D > 0) ? (1.0 / (2.0 * D * denom_diff)) : std::numeric_limits<double>::infinity();

    return std::min(dt_adv, dt_diff);
}

// Use extended-precision rates so tiny spacings do not underflow when squared.
inline double combined_dt(
    double dx, double dy, double vx, double vy, double D, double dz, double vz, double Dz) {
    const long double x = dx, y = dy, z = dz;
    const long double adv = std::abs(vx) / x + std::abs(vy) / y + std::abs(vz) / z;
    const long double diff = (D / x) / x + (D / y) / y + (Dz / z) / z;
    const long double rate = adv + 2 * diff;
    return rate == 0 ? std::numeric_limits<double>::infinity() : static_cast<double>(1 / rate);
}
