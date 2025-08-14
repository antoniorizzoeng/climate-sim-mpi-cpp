#include "decomp.hpp"

#include <mpi.h>

void Decomp2D::init(MPI_Comm comm_world, int nxg, int nyg) {
    nx_global = nxg;
    ny_global = nyg;

    int size = 0, world_rank = 0;
    MPI_Comm_size(comm_world, &size);
    MPI_Comm_rank(comm_world, &world_rank);

    MPI_Dims_create(size, 2, dims);
    int periods[2] = {0, 0};
    MPI_Cart_create(comm_world, 2, dims, periods, 0, &cart_comm);

    int cart_rank = 0;
    MPI_Comm_rank(cart_comm, &cart_rank);
    MPI_Cart_coords(cart_comm, cart_rank, 2, coords);

    MPI_Cart_shift(cart_comm, 0, 1, &nbr_lr[0], &nbr_lr[1]);
    MPI_Cart_shift(cart_comm, 1, 1, &nbr_du[0], &nbr_du[1]);

    const int base_nx = nx_global / dims[0];
    const int base_ny = ny_global / dims[1];
    const int rem_x = nx_global % dims[0];
    const int rem_y = ny_global % dims[1];

    nx_local = base_nx + (coords[0] == dims[0] - 1 ? rem_x : 0);
    ny_local = base_ny + (coords[1] == dims[1] - 1 ? rem_y : 0);

    x_offset = coords[0] * base_nx;
    y_offset = coords[1] * base_ny;
}

void Decomp2D::finalize() {
    if (cart_comm != MPI_COMM_NULL)
        MPI_Comm_free(&cart_comm);
}
