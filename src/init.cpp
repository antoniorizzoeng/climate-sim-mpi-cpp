#include "init.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

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

static void ic_from_bin_global(const Decomp2D& dec, Field& u, const SimConfig& cfg) {
    const int NX = cfg.nx, NY = cfg.ny;
    std::ifstream ifs(cfg.ic.path, std::ios::binary);
    if (!ifs)
        throw std::runtime_error("IC file open failed: " + cfg.ic.path);

    const int h = u.halo;
    const int nx = u.nx_local;
    const int ny = u.ny_local;

    for (int j = 0; j < ny; ++j) {
        const int gj = dec.y_offset + j;
        const std::streamoff row_off = static_cast<std::streamoff>(gj) * NX * sizeof(double);
        const std::streamoff col_off = static_cast<std::streamoff>(dec.x_offset) * sizeof(double);
        ifs.seekg(row_off + col_off, std::ios::beg);
        for (int i = 0; i < nx; ++i) {
            double val{};
            ifs.read(reinterpret_cast<char*>(&val), sizeof(double));
            u.at(h + i, h + j) = val;
        }
        if (!ifs)
            throw std::runtime_error("IC file read failed at row " + std::to_string(gj));
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
    } else if (cfg.ic.mode == "file") {
        if (cfg.ic.format == "bin") {
            ic_from_bin_global(dec, u, cfg);
        } else {
            throw std::runtime_error("Unsupported IC format: " + cfg.ic.format);
        }
    } else {
        throw std::runtime_error("Unknown IC mode: " + cfg.ic.mode);
    }
}
