#include "boundary.hpp"

#include <algorithm>

static inline void fill_col(Field& f, int i, int j0, int j1, double v) {
    double* col = &f.at(i, j0);
    const size_t pitch = f.nx_total();
    for (int j = 0; j <= j1 - j0; ++j) col[j * pitch] = v;
}
static inline void fill_row(Field& f, int j, int i0, int i1, double v) {
    std::fill_n(&f.at(i0, j), i1 - i0 + 1, v);
}

void apply_boundary(Field& f, const Decomp2D& dec, const BCConfig& bc, double value = 0.0) {
    const int h = f.halo;
    const int nx = f.nx_local;
    const int ny = f.ny_local;
    const int iL = 0;
    const int iR = h + nx;
    const int jB = 0;
    const int jT = h + ny;
    const int i0 = 0;
    const int i1 = f.nx_total() - 1;

    if (dec.nbr_lr[0] == MPI_PROC_NULL) {
        if (bc.left == BCType::Dirichlet) {
            fill_col(f, iL, jB, jT, value);
        } else if (bc.left == BCType::Neumann) {
            for (int j = jB; j <= jT; ++j) f.at(iL, j) = f.at(h, j);
        }
    }

    if (dec.nbr_lr[1] == MPI_PROC_NULL) {
        if (bc.right == BCType::Dirichlet) {
            fill_col(f, iR, jB, jT, value);
        } else if (bc.right == BCType::Neumann) {
            for (int j = jB; j <= jT; ++j) f.at(iR, j) = f.at(h + nx - 1, j);
        }
    }

    if (dec.nbr_du[0] == MPI_PROC_NULL) {
        if (bc.bottom == BCType::Dirichlet) {
            fill_row(f, jB, i0, i1, value);
        } else if (bc.bottom == BCType::Neumann) {
            std::copy_n(&f.at(i0, h), f.nx_total(), &f.at(i0, jB));
        }
    }

    if (dec.nbr_du[1] == MPI_PROC_NULL) {
        if (bc.top == BCType::Dirichlet) {
            fill_row(f, jT, i0, i1, value);
        } else if (bc.top == BCType::Neumann) {
            std::copy_n(&f.at(i0, h + ny - 1), f.nx_total(), &f.at(i0, jT));
        }
    }
}
