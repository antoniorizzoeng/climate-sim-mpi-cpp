#include "halo.hpp"

#include <algorithm>
#include <array>

void exchange_halos(Field& f, const Decomp2D& dec, MPI_Comm comm) {
    const int h = f.halo;
    const int nx = f.nx_local;
    const int ny = f.ny_local;
    const int pitch = f.nx_total();
    if (dec.nbr_lr[0] == MPI_PROC_NULL && dec.nbr_lr[1] == MPI_PROC_NULL &&
        dec.nbr_du[0] == MPI_PROC_NULL && dec.nbr_du[1] == MPI_PROC_NULL)
        return;

    std::array<MPI_Request, 8> requests;
    int pending = 0;
    auto face =
        [&](int peer, int tag, const double* send, double* recv, int count, MPI_Datatype type) {
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
    auto send_at = [&](int i, int j) {
        return saved.empty() ? &f.at(i, j) : &saved[static_cast<size_t>(j) * pitch + i];
    };

    // When the process grid exceeds the domain along an axis, the last coordinate
    // owns its entire extent and can wrap locally just like a single-rank axis.
    if (dec.periods[0] && (dec.dims[0] == 1 || dec.nx_global < dec.dims[0])) {
        for (int j = h; j < h + ny; ++j) {
            f.at(0, j) = f.at(h + nx - 1, j);
            f.at(h + nx, j) = f.at(h, j);
        }
    } else if (dec.nbr_lr[0] != MPI_PROC_NULL || dec.nbr_lr[1] != MPI_PROC_NULL) {
        MPI_Datatype col;
        MPI_Type_vector(ny, 1, pitch, MPI_DOUBLE, &col);
        MPI_Type_commit(&col);
        face(dec.nbr_lr[0], 0, send_at(h, h), &f.at(0, h), 1, col);
        face(dec.nbr_lr[1], 1, send_at(h + nx - 1, h), &f.at(h + nx, h), 1, col);
        MPI_Type_free(&col);
    }

    // Periodic corners travel with the full y rows after x arrives. For a
    // nonperiodic five-point stencil all four disjoint faces can be in flight together.
    const bool full_rows = dec.periods[0] || dec.periods[1];
    if (full_rows)
        wait();
    if (dec.periods[1] && (dec.dims[1] == 1 || dec.ny_global < dec.dims[1])) {
        if (ny > 0) {
            std::copy_n(&f.at(0, h + ny - 1), pitch, &f.at(0, 0));
            std::copy_n(&f.at(0, h), pitch, &f.at(0, h + ny));
        }
    } else if (dec.nbr_du[0] != MPI_PROC_NULL || dec.nbr_du[1] != MPI_PROC_NULL) {
        const int i = full_rows ? 0 : h;
        const int count = full_rows ? pitch : nx;
        face(dec.nbr_du[0], 2, send_at(i, h), &f.at(i, 0), count, MPI_DOUBLE);
        face(dec.nbr_du[1], 3, send_at(i, h + ny - 1), &f.at(i, h + ny), count, MPI_DOUBLE);
    }
    wait();
}
