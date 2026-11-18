#include "step.hpp"

#include <algorithm>

#include "advection.hpp"
#include "diffusion.hpp"
#include "halo.hpp"

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

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int j = u.halo; j < u.halo + u.ny_local; ++j) {
        const double* row = &u.at(u.halo, j);
        double* out = &scratch.at(u.halo, j);
        for (int i = 0; i < u.nx_local; ++i) {
            const double center = row[i];
            const double x = west * (row[i - 1] - center) + east * (row[i + 1] - center);
            const double y =
                south * ((row - pitch)[i] - center) + north * ((row + pitch)[i] - center);
            out[i] = center + (x + y);
        }
    }
    u.data.swap(scratch.data);
}
