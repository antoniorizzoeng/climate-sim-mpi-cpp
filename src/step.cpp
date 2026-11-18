#include "step.hpp"

#include <algorithm>
#include <cmath>

#include "advection.hpp"
#include "diffusion.hpp"
#include "halo.hpp"
#include "stencil_math.hpp"

namespace {
using namespace stencil_math;

inline double velocity(const std::vector<double>& lo,
                       const std::vector<double>& hi,
                       size_t i,
                       double a) {
    return interpolate(lo[i], hi[i], a);
}

// The compiler emits separate five- and seven-point loops. File velocities
// are indexed by the unpadded tile offset; scalar neighbors by padded strides.
template <bool volume, bool file>
void stencil(Field& u, Field& scratch, const SimConfig& cfg, const WindSample* wind) {
    constexpr bool robust = file || volume;
    const double diff_x = diffusion<robust>(cfg.dt, cfg.D, u.dx);
    const double diff_y = diffusion<robust>(cfg.dt, cfg.D, u.dy);
    const double diff_z = volume ? diffusion<true>(cfg.dt, cfg.vertical_diffusion(), u.dz) : 0.0;
    const Weights uniform_x = upwind(diff_x, cfg.dt * cfg.vx / u.dx);
    const Weights uniform_y = upwind(diff_y, cfg.dt * cfg.vy / u.dy);
    const Weights uniform_z = volume ? upwind(diff_z, cfg.dt * cfg.vz / u.dz) : Weights{};
    const int nx = u.nx_local, ny = u.ny_local, pitch = u.nx_total();
    const std::ptrdiff_t plane = u.plane_size();
    const size_t origin = u.idx(u.halo, u.halo, u.z_halo);
    const double* interior = u.data.data() + origin;
    double* destination = scratch.data.data() + origin;
    const int slices = volume ? u.nz_local : 1;

#ifdef _OPENMP
#pragma omp parallel for collapse(2) schedule(static)
#endif
    for (int k = 0; k < slices; ++k) {
        for (int j = 0; j < ny; ++j) {
            const size_t start = k * plane + static_cast<size_t>(j) * pitch;
            const double* row = interior + start;
            double* out = destination + start;
            const size_t offset = (static_cast<size_t>(k) * ny + j) * nx;
#if defined(__GNUC__) && (__GNUC__ >= 8 || defined(__clang__))
#pragma GCC unroll 4
#endif
            for (int i = 0; i < nx; ++i) {
                Weights x = uniform_x, y = uniform_y, z = uniform_z;
                if constexpr (file) {
                    const auto cell = offset + i;
                    const double vx =
                        velocity(wind->lower.vx, wind->upper.vx, cell, wind->fraction);
                    const double vy =
                        velocity(wind->lower.vy, wind->upper.vy, cell, wind->fraction);
                    x = upwind(diff_x, cfg.dt * vx / u.dx);
                    y = upwind(diff_y, cfg.dt * vy / u.dy);
                    if constexpr (volume) {
                        const double vz =
                            wind->lower.vz.empty()
                                ? 0
                                : velocity(wind->lower.vz, wind->upper.vz, cell, wind->fraction);
                        z = upwind(diff_z, cfg.dt * vz / u.dz);
                    }
                }
                const double* cell = row + i;
                double delta = difference(cell, 1, x) + difference(cell, pitch, y);
                if constexpr (volume)
                    delta += difference(cell, plane, z);
                out[i] = *cell + delta;
            }
        }
    }
    u.data.swap(scratch.data);
}

template <bool file>
void advance(Field& u,
             Field& scratch,
             const Decomposition& dec,
             const SimConfig& cfg,
             const WindSample* wind) {
    exchange_halos(u, dec, dec.cart_comm);
    apply_boundary(u, dec, cfg.bc, 0.0);
    if constexpr (!file) {
        if (dec.nx_global < dec.dims[0] || dec.ny_global < dec.dims[1]) {
            // Legacy empty 2D tiles relay their previous ghost on the next step.
            std::copy(u.data.begin(), u.data.end(), scratch.data.begin());
            diffusion_step(u, scratch, cfg.D, cfg.dt);
            advection_step(u, scratch, cfg.vx, cfg.vy, cfg.dt);
            u.data.swap(scratch.data);
            return;
        }
    }
    if (!u.nx_local || !u.ny_local || !u.nz_local)
        return;
    if (u.z_halo)
        stencil<true, file>(u, scratch, cfg, wind);
    else
        stencil<false, file>(u, scratch, cfg, wind);
}
}  // namespace

void advance_field(Field& u, Field& scratch, const Decomposition& dec, const SimConfig& cfg) {
    advance<false>(u, scratch, dec, cfg, nullptr);
}
void advance_field(
    Field& u, Field& scratch, const Decomposition& dec, const SimConfig& cfg, WindSample wind) {
    advance<true>(u, scratch, dec, cfg, &wind);
}
