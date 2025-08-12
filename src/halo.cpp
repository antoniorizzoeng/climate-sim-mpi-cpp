#include "halo.hpp"
#include <array>
#include <stdexcept>

void exchange_halos(Field& f, const Decomp2D& dec, MPI_Comm comm)
{
    const int h  = f.halo;
    const int nx = f.nx_local;
    const int ny = f.ny_local;
    const int nx_tot = f.nx_total();

    MPI_Datatype colType;
    MPI_Type_vector(ny, 1, nx_tot, MPI_DOUBLE, &colType);
    MPI_Type_commit(&colType);

    MPI_Datatype rowType;
    MPI_Type_contiguous(nx_tot, MPI_DOUBLE, &rowType);
    MPI_Type_commit(&rowType);

    std::array<MPI_Request, 8> req{};
    int rcount = 0;

    const int left  = dec.nbr_lr[0];
    const int right = dec.nbr_lr[1];
    const int down  = dec.nbr_du[0];
    const int up    = dec.nbr_du[1];

    if (left != MPI_PROC_NULL) {
        MPI_Irecv(&f.at(0,   h), 1, colType, left,  100, comm, &req[rcount++]);
        MPI_Isend(&f.at(h,   h), 1, colType, left,  101, comm, &req[rcount++]);
    }
    if (right != MPI_PROC_NULL) {
        MPI_Irecv(&f.at(h+nx, h), 1, colType, right, 101, comm, &req[rcount++]);
        MPI_Isend(&f.at(h+nx-1,h), 1, colType, right, 100, comm, &req[rcount++]);
    }
    if (down != MPI_PROC_NULL) {
        MPI_Irecv(&f.at(0, 0), 1, rowType, down, 200, comm, &req[rcount++]);
        MPI_Isend(&f.at(0, h), 1, rowType, down, 201, comm, &req[rcount++]);
    }
    if (up != MPI_PROC_NULL) {
        MPI_Irecv(&f.at(0, h+ny),   1, rowType, up,   201, comm, &req[rcount++]);
        MPI_Isend(&f.at(0, h+ny-1), 1, rowType, up,   200, comm, &req[rcount++]);
    }

    if (rcount) MPI_Waitall(rcount, req.data(), MPI_STATUSES_IGNORE);

    MPI_Type_free(&colType);
    MPI_Type_free(&rowType);
}
