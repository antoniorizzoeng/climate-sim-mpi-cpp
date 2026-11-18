#include "boundary.hpp"

#include <algorithm>

void apply_boundary(Field& f, const Decomposition& dec, const BCConfig& bc, double value) {
    const int pitch = f.nx_total(), h = f.halo;
    const size_t plane = f.plane_size();
    auto fill_face = [&](double* ghost, const double* inside, size_t count, BCType type) {
        if (type == BCType::Dirichlet)
            std::fill_n(ghost, count, value);
        else if (type == BCType::Neumann)
            std::copy_n(inside, count, ghost);
    };
    for (int k = f.z_halo; k < f.z_halo + f.nz_local; ++k) {
        double* slice = f.data.data() + k * plane;
        auto column = [&](int i, int interior, BCType type) {
            for (int j = 0; j < f.ny_total(); ++j) {
                double* row = slice + static_cast<size_t>(j) * pitch;
                if (type == BCType::Dirichlet)
                    row[i] = value;
                else if (type == BCType::Neumann)
                    row[i] = row[interior];
            }
        };
        if (dec.nbr_lr[0] == MPI_PROC_NULL)
            column(0, h, bc.left);
        if (dec.nbr_lr[1] == MPI_PROC_NULL)
            column(h + f.nx_local, h + f.nx_local - 1, bc.right);
        if (dec.nbr_du[0] == MPI_PROC_NULL)
            fill_face(slice, slice + h * pitch, pitch, bc.bottom);
        if (dec.nbr_du[1] == MPI_PROC_NULL) {
            double* top = slice + static_cast<size_t>(h + f.ny_local) * pitch;
            fill_face(top, top - pitch, pitch, bc.top);
        }
    }
    if (f.z_halo && f.nz_local) {
        if (dec.nbr_fb[0] == MPI_PROC_NULL)
            fill_face(f.data.data(), f.data.data() + plane, plane, bc.front);
        if (dec.nbr_fb[1] == MPI_PROC_NULL) {
            double* back = f.data.data() + (h + f.nz_local) * plane;
            fill_face(back, back - plane, plane, bc.back);
        }
    }
}
