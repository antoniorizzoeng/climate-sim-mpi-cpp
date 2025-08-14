#include "diffusion.hpp"

void diffusion_step(const Field& u, Field& out, double D, double dt) {
    const double dx = u.dx;
    const double dy = u.dy;
    const double ax = D * dt / (dx * dx);
    const double ay = D * dt / (dy * dy);

    for (int j = u.halo; j < u.ny_local + u.halo; ++j) {
        for (int i = u.halo; i < u.nx_local + u.halo; ++i) {
            const double uij = u.at(i, j);
            const double lap = (u.at(i + 1, j) - 2.0 * uij + u.at(i - 1, j)) / (dx * dx) +
                               (u.at(i, j + 1) - 2.0 * uij + u.at(i, j - 1)) / (dy * dy);
            out.at(i, j) = uij + dt * D * lap;
        }
    }

    for (int i = 0; i < u.nx_total(); ++i) {
        out.at(i, 0) = u.at(i, 0);
        out.at(i, u.ny_total() - 1) = u.at(i, u.ny_total() - 1);
    }
    for (int j = 0; j < u.ny_total(); ++j) {
        out.at(0, j) = u.at(0, j);
        out.at(u.nx_total() - 1, j) = u.at(u.nx_total() - 1, j);
    }
}
