#include "halo.hpp"

#include <algorithm>
#include <array>

void exchange_halos(Field& f, const Decomposition& dec, MPI_Comm comm) {
    const int h = f.halo;
    const int nx = f.nx_local;
    const int ny = f.ny_local;
    const int pitch = f.nx_total();
    if (dec.nbr_lr[0] == MPI_PROC_NULL && dec.nbr_lr[1] == MPI_PROC_NULL &&
        dec.nbr_du[0] == MPI_PROC_NULL && dec.nbr_du[1] == MPI_PROC_NULL &&
        dec.nbr_fb[0] == MPI_PROC_NULL && dec.nbr_fb[1] == MPI_PROC_NULL)
        return;

    std::array<MPI_Request, 12> requests;
    int pending = 0;
    auto face = [&](int peer, int tag, const double* send, double* recv, int axis) {
        const auto count = dec.face_count[axis];
        const auto type = dec.face_type[axis];
        if (peer != MPI_PROC_NULL) {
            MPI_Irecv(recv, count, type, peer, tag ^ 1, comm, &requests[pending++]);
            MPI_Isend(send, count, type, peer, tag, comm, &requests[pending++]);
        }
    };
    auto wait = [&] {
        if (pending)
            MPI_Waitall(pending, requests.data(), MPI_STATUSES_IGNORE);
        pending = 0;
    };

    // Empty tiles relay their previous ghosts, which can also be receive buffers.
    const std::vector<double> saved = nx == 0 || ny == 0 ? f.data : std::vector<double>{};
    const size_t plane = f.plane_size();
    const int kz = f.z_halo, nz = f.nz_local;
    auto at = [&](int i, int j, int k) {
        return f.data.data() + k * plane + static_cast<size_t>(j) * pitch + i;
    };
    auto send_at = [&](int i, int j) {
        return saved.empty() ? at(i, j, kz) : saved.data() + static_cast<size_t>(j) * pitch + i;
    };
    // When the process grid exceeds the domain along an axis, the last coordinate
    // owns its entire extent and can wrap locally just like a single-rank axis.
    if (dec.periods[0] && (dec.dims[0] == 1 || dec.nx_global < dec.dims[0])) {
        for (int k = kz; k < kz + nz; ++k)
            for (int j = h; j < h + ny; ++j) {
                *at(0, j, k) = *at(h + nx - 1, j, k);
                *at(h + nx, j, k) = *at(h, j, k);
            }
    } else if (dec.nbr_lr[0] != MPI_PROC_NULL || dec.nbr_lr[1] != MPI_PROC_NULL) {
        face(dec.nbr_lr[0], 0, send_at(h, h), at(0, h, kz), 0);
        face(dec.nbr_lr[1], 1, send_at(h + nx - 1, h), at(h + nx, h, kz), 0);
    }

    // Periodic corners travel with the full y rows after x arrives. For a
    // nonperiodic stencil all disjoint receive faces can be in flight together.
    const bool full_rows = dec.periods[0] || dec.periods[1] || dec.periods[2];
    if (full_rows)
        wait();
    if (dec.periods[1] && (dec.dims[1] == 1 || dec.ny_global < dec.dims[1])) {
        if (ny > 0)
            for (int k = kz; k < kz + nz; ++k) {
                std::copy_n(at(0, h + ny - 1, k), pitch, at(0, 0, k));
                std::copy_n(at(0, h, k), pitch, at(0, h + ny, k));
            }
    } else if (dec.nbr_du[0] != MPI_PROC_NULL || dec.nbr_du[1] != MPI_PROC_NULL) {
        const int i = full_rows ? 0 : h;
        face(dec.nbr_du[0], 2, send_at(i, h), at(i, 0, kz), 1);
        face(dec.nbr_du[1], 3, send_at(i, h + ny - 1), at(i, h + ny, kz), 1);
    }
    // Complete x and y before sending planes, so wrapped edges and corners travel with z.
    if (dec.nbr_fb[0] != MPI_PROC_NULL || dec.nbr_fb[1] != MPI_PROC_NULL) {
        if (full_rows)
            wait();
        if (dec.dims[2] == 1) {
            std::copy_n(at(0, 0, nz), plane, at(0, 0, 0));
            std::copy_n(at(0, 0, kz), plane, at(0, 0, kz + nz));
        } else {
            const int i = full_rows ? 0 : h;
            const int j = full_rows ? 0 : h;
            face(dec.nbr_fb[0], 4, at(i, j, kz), at(i, j, 0), 2);
            face(dec.nbr_fb[1], 5, at(i, j, kz + nz - 1), at(i, j, kz + nz), 2);
        }
    }
    wait();
}
