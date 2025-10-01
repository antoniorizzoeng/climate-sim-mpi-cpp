#include "init.hpp"

#include <pnetcdf.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static void ic_gaussian(const Decomp2D& dec, Field& u, const SimConfig& cfg) {
    const int h = u.halo;
    const int nx = u.nx_local;
    const int ny = u.ny_local;

    const double Lx = cfg.nx * cfg.dx;
    const double Ly = cfg.ny * cfg.dy;
    const double xc = cfg.ic.xc_frac * Lx;
    const double yc = cfg.ic.yc_frac * Ly;
    const double sig = cfg.ic.sigma_frac * std::min(Lx, Ly);

    for (int j = h; j < h + ny; ++j) {
        const int gj = dec.y_offset + (j - h);
        const double y = (gj + 0.5) * cfg.dy;
        for (int i = h; i < h + nx; ++i) {
            const int gi = dec.x_offset + (i - h);
            const double x = (gi + 0.5) * cfg.dx;
            const double r2 = (x - xc) * (x - xc) + (y - yc) * (y - yc);
            u.at(i, j) = cfg.ic.A * std::exp(-r2 / (2.0 * sig * sig));
        }
    }
}

void apply_initial_condition(const Decomp2D& dec, Field& u, const SimConfig& cfg) {
    if (cfg.ic.mode == "preset") {
        if (cfg.ic.preset == "gaussian_hotspot") {
            ic_gaussian(dec, u, cfg);
        } else if (cfg.ic.preset == "constant_zero") {
            /* no-op, u already zero */
        } else {
            throw std::runtime_error("Unknown IC preset: " + cfg.ic.preset);
        }
    } else {
        throw std::runtime_error("IC mode 'file' not supported in PnetCDF build.");
    }
}
