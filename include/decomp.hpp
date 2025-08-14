#pragma once
#include <mpi.h>

struct Decomp2D {
    MPI_Comm cart_comm = MPI_COMM_NULL;
    int dims[2]{0, 0};
    int coords[2]{0, 0};
    int nbr_lr[2]{MPI_PROC_NULL, MPI_PROC_NULL};
    int nbr_du[2]{MPI_PROC_NULL, MPI_PROC_NULL};

    int nx_global = 0, ny_global = 0;
    int nx_local = 0, ny_local = 0;
    int x_offset = 0, y_offset = 0;

    void init(MPI_Comm comm_world, int nx_global_, int ny_global_);
    void finalize();
};
