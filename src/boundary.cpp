#include "boundary.hpp"
#include <algorithm>

static inline void fill_col(Field& f, int i, int j0, int j1, double v) {
    for (int j = j0; j <= j1; ++j) f.at(i, j) = v;
}
static inline void fill_row(Field& f, int j, int i0, int i1, double v) {
    for (int i = i0; i <= i1; ++i) f.at(i, j) = v;
}

void apply_boundary(Field& f, const Decomp2D& dec, BCType bc, double value)
{
    const int h  = f.halo;
    const int nx = f.nx_local;
    const int ny = f.ny_local;
    const int iL = 0;
    const int iR = h + nx;
    const int jB = 0;
    const int jT = h + ny;
    const int i0 = 0;
    const int i1 = f.nx_total() - 1;

    // assume MPI Cartesian is periodic.
    if (bc == BCType::Periodic) return;

    if (dec.nbr_lr[0] == MPI_PROC_NULL) {
        if (bc == BCType::Dirichlet) {
            fill_col(f, iL, jB, jT, value);
        } else {
            for (int j = jB; j <= jT; ++j) f.at(iL, j) = f.at(h, j);
        }
    }
    if (dec.nbr_lr[1] == MPI_PROC_NULL) {
        if (bc == BCType::Dirichlet) {
            fill_col(f, iR, jB, jT, value);
        } else {
            for (int j = jB; j <= jT; ++j) f.at(iR, j) = f.at(h + nx - 1, j);
        }
    }
    if (dec.nbr_du[0] == MPI_PROC_NULL) {
        if (bc == BCType::Dirichlet) {
            fill_row(f, jB, i0, i1, value);
        } else {
            for (int i = i0; i <= i1; ++i) f.at(i, jB) = f.at(i, h);
        }
    }
    if (dec.nbr_du[1] == MPI_PROC_NULL) {
        if (bc == BCType::Dirichlet) {
            fill_row(f, jT, i0, i1, value);
        } else {
            for (int i = i0; i <= i1; ++i) f.at(i, jT) = f.at(i, h + ny - 1);
        }
    }
}
