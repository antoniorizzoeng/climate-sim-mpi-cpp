#include "step.hpp"

#include <algorithm>
#include <cmath>

#include "advection.hpp"
#include "diffusion.hpp"
#include "halo.hpp"

namespace {
inline double stencil(
    const double* row, int i, int pitch, double west, double east, double south, double north) {
    const double center = row[i];
    const double x = west * (row[i - 1] - center) + east * (row[i + 1] - center);
    const double y = south * ((row - pitch)[i] - center) + north * ((row + pitch)[i] - center);
    return center + (x + y);
}
}  // namespace

void advance_field(Field& u, Field& scratch, const Decomp2D& dec, const SimConfig& cfg) {
    exchange_halos(u, dec, dec.cart_comm);
    apply_boundary(u, dec, cfg.bc, 0.0);

    // When a preset grid is smaller than its process grid, empty tiles send
    // ghost values onward. Keep the original full-buffer step in this case.
    if (dec.nx_global < dec.dims[0] || dec.ny_global < dec.dims[1]) {
        std::copy(u.data.begin(), u.data.end(), scratch.data.begin());
        diffusion_step(u, scratch, cfg.D, cfg.dt);
        advection_step(u, scratch, cfg.vx, cfg.vy, cfg.dt);
        u.data.swap(scratch.data);
        return;
    }

    const double diff_x = cfg.dt * cfg.D / (u.dx * u.dx);
    const double diff_y = cfg.dt * cfg.D / (u.dy * u.dy);
    const double adv_x = cfg.dt * cfg.vx / u.dx;
    const double adv_y = cfg.dt * cfg.vy / u.dy;
    // Upwind advection adds weight only to the upstream neighbor on each axis.
    const double west = diff_x + std::max(adv_x, 0.0);
    const double east = diff_x - std::min(adv_x, 0.0);
    const double south = diff_y + std::max(adv_y, 0.0);
    const double north = diff_y - std::min(adv_y, 0.0);
    const int pitch = u.nx_total();
    const double* interior = &u.at(u.halo, u.halo);
    double* destination = &scratch.at(u.halo, u.halo);

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int j = 0; j < u.ny_local; ++j) {
        const size_t start = static_cast<size_t>(j) * pitch;
        const double* row = interior + start;
        double* out = destination + start;
// Keep independent neighboring updates in flight on the portable build.
#if defined(__GNUC__) && (__GNUC__ >= 8 || defined(__clang__))
#pragma GCC unroll 4
#endif
        for (int i = 0; i < u.nx_local; ++i) {
            out[i] = stencil(row, i, pitch, west, east, south, north);
        }
    }
    u.data.swap(scratch.data);
}

void advance_field(
    Field& u, Field& scratch, const Decomp2D& dec, const SimConfig& cfg, WindSample wind) {
    exchange_halos(u, dec, dec.cart_comm);
    apply_boundary(u, dec, cfg.bc, 0.0);
    if (u.nx_local == 0 || u.ny_local == 0)
        return;

    const double dt_D = cfg.dt * cfg.D;
    auto diffusion_weight = [&](double spacing) {
        // Use the uniform kernel's arithmetic whenever its intermediates are representable.
        const double square = spacing * spacing;
        if (std::isnormal(square) && (cfg.D == 0 || std::isnormal(dt_D)))
            return dt_D / square;
        return static_cast<double>((static_cast<long double>(cfg.dt) * cfg.D / spacing) / spacing);
    };
    const double diff_x = diffusion_weight(u.dx), diff_y = diffusion_weight(u.dy);
    const double a = wind.fraction, b = 1.0 - a;
    auto interpolate = [a, b](double lo, double hi) { return lo == hi ? lo : b * lo + a * hi; };
    const int pitch = u.nx_total();
    const double* interior = &u.at(u.halo, u.halo);
    double* destination = &scratch.at(u.halo, u.halo);

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int j = 0; j < u.ny_local; ++j) {
        const size_t start = static_cast<size_t>(j) * pitch;
        const double* row = interior + start;
        double* out = destination + start;
        const size_t offset = static_cast<size_t>(j) * u.nx_local;
        for (int i = 0; i < u.nx_local; ++i) {
            const auto k = offset + i;
            const double vx = interpolate(wind.lower.vx[k], wind.upper.vx[k]);
            const double vy = interpolate(wind.lower.vy[k], wind.upper.vy[k]);
            const double adv_x = cfg.dt * vx / u.dx;
            const double adv_y = cfg.dt * vy / u.dy;
            out[i] = stencil(row,
                             i,
                             pitch,
                             diff_x + std::max(adv_x, 0.0),
                             diff_x - std::min(adv_x, 0.0),
                             diff_y + std::max(adv_y, 0.0),
                             diff_y - std::min(adv_y, 0.0));
        }
    }
    u.data.swap(scratch.data);
}
