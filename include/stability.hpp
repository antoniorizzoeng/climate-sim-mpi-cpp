#pragma once
#include <cmath>
#include <limits>

// Use extended-precision rates so tiny spacings do not underflow when squared.
inline double combined_dt(
    double dx, double dy, double vx, double vy, double D, double dz, double vz, double Dz) {
    const long double x = dx, y = dy, z = dz;
    const long double adv = std::abs(vx) / x + std::abs(vy) / y + std::abs(vz) / z;
    const long double diff = (D / x) / x + (D / y) / y + (Dz / z) / z;
    const long double rate = adv + 2 * diff;
    return rate == 0 ? std::numeric_limits<double>::infinity() : static_cast<double>(1 / rate);
}

inline double safe_dt(double dx, double dy, double vx, double vy, double D) {
    // The 2D CLI historically applies a diffusion limit only for positive D.
    return combined_dt(dx, dy, vx, vy, D > 0 ? D : 0, 1.0, 0.0, 0.0);
}
