#include "advection.hpp"

#include <algorithm>

void advection_step(const Field& u, Field& out, double vx, double vy, double dt) {
    const int h = u.halo;
    const int nx = u.nx_local;
    const int ny = u.ny_local;

    const double dx = u.dx;
    const double dy = u.dy;

    for (int j = h; j < h + ny; ++j) {
        for (int i = h; i < h + nx; ++i) {
            double dudx;
            if (vx >= 0.0) {
                dudx = (u.at(i, j) - u.at(i - 1, j)) / dx;
            } else {
                dudx = (u.at(i + 1, j) - u.at(i, j)) / dx;
            }

            double dudy;
            if (vy >= 0.0) {
                dudy = (u.at(i, j) - u.at(i, j - 1)) / dy;
            } else {
                dudy = (u.at(i, j + 1) - u.at(i, j)) / dy;
            }

            const double adv = vx * dudx + vy * dudy;

            out.at(i, j) += (-dt) * adv;
        }
    }
}
