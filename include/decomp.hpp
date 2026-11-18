#pragma once
#include <mpi.h>

struct Decomposition {
    MPI_Comm cart_comm = MPI_COMM_NULL;
    MPI_Comm io_comm = MPI_COMM_NULL;  // Includes ranks with an empty I/O hyperslab.
    int dims[3]{}, periods[3]{}, coords[3]{};
    int nbr_lr[2]{MPI_PROC_NULL, MPI_PROC_NULL};
    int nbr_du[2]{MPI_PROC_NULL, MPI_PROC_NULL};
    int nbr_fb[2]{MPI_PROC_NULL, MPI_PROC_NULL};

    int nx_global = 0, ny_global = 0, nz_global = 1;
    int nx_local = 0, ny_local = 0, nz_local = 1;
    int x_offset = 0, y_offset = 0, z_offset = 0;

    void init(MPI_Comm comm, int nx, int ny, bool periodic_x = false, bool periodic_y = false) {
        init(comm, nx, ny, 1, periodic_x, periodic_y, false);
    }
    void init(MPI_Comm comm,
              int nx,
              int ny,
              int nz,
              bool periodic_x = false,
              bool periodic_y = false,
              bool periodic_z = false);
    // Datatypes for the one-cell stencil halo. Faces use x, y, z order.
    MPI_Datatype face_type[3]{MPI_DATATYPE_NULL, MPI_DATATYPE_NULL, MPI_DATATYPE_NULL};
    int face_count[3]{1, 1, 1};
    void finalize();

   private:
    void prepare_faces();
};

using Decomp2D = Decomposition;
