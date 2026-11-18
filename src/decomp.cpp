#include "decomp.hpp"

#include <algorithm>

void Decomposition::init(MPI_Comm comm, int nx, int ny, int nz, bool px, bool py, bool pz) {
    io_comm = comm;
    std::fill_n(face_count, 3, 1);
    nx_global = nx;
    ny_global = ny;
    nz_global = nz;
    const int extent[] = {nx, ny, nz};
    int size;
    MPI_Comm_size(comm, &size);
    // Keep the default Cartesian layout if it fits. Otherwise use the largest
    // populated grid possible and leave surplus ranks in collective I/O only.
    dims[0] = nz > 1 && nx == 1;
    dims[1] = nz > 1 && ny == 1;
    dims[2] = nz == 1;
    MPI_Dims_create(size, 3, dims);
    if (dims[0] > nx || dims[1] > ny || dims[2] > nz) {
        int best = 0;
        for (int x = 1; x <= std::min(nx, size) && best < size; ++x) {
            for (int y = 1; y <= std::min(ny, size / x) && best < size; ++y) {
                const int z = std::min(nz, size / (x * y));
                const int tiles = x * y * z;
                if (tiles > best) {
                    best = tiles;
                    dims[0] = x;
                    dims[1] = y;
                    dims[2] = z;
                }
            }
        }
    }
    periods[0] = px;
    periods[1] = py;
    periods[2] = nz > 1 && pz;
    MPI_Cart_create(comm, 3, dims, periods, 0, &cart_comm);
    if (cart_comm == MPI_COMM_NULL) {
        nx_local = ny_local = nz_local = 0;
        std::fill_n(nbr_lr, 2, MPI_PROC_NULL);
        std::fill_n(nbr_du, 2, MPI_PROC_NULL);
        std::fill_n(nbr_fb, 2, MPI_PROC_NULL);
        return;
    }
    int rank;
    MPI_Comm_rank(cart_comm, &rank);
    MPI_Cart_coords(cart_comm, rank, 3, coords);
    int* neighbors[] = {nbr_lr, nbr_du, nbr_fb};
    int* local[] = {&nx_local, &ny_local, &nz_local};
    int* offset[] = {&x_offset, &y_offset, &z_offset};
    for (int axis = 0; axis < 3; ++axis) {
        MPI_Cart_shift(cart_comm, axis, 1, &neighbors[axis][0], &neighbors[axis][1]);
        const int base = extent[axis] / dims[axis];
        *offset[axis] = coords[axis] * base;
        *local[axis] = base + (coords[axis] == dims[axis] - 1 ? extent[axis] % dims[axis] : 0);
    }
    prepare_faces();
}

void Decomposition::prepare_faces() {
    const int pitch = nx_local + 2, rows = ny_local + 2;
    const bool periodic = periods[0] || periods[1] || periods[2];
    const MPI_Aint plane_bytes = static_cast<MPI_Aint>(pitch) * rows * sizeof(double);
    if (nbr_lr[0] != MPI_PROC_NULL || nbr_lr[1] != MPI_PROC_NULL) {
        MPI_Type_vector(ny_local, 1, pitch, MPI_DOUBLE, &face_type[0]);
        if (nz_local > 1) {
            MPI_Datatype column = face_type[0];
            MPI_Type_create_hvector(nz_local, 1, plane_bytes, column, &face_type[0]);
            MPI_Type_free(&column);
        }
        MPI_Type_commit(&face_type[0]);
    }
    if (nbr_du[0] != MPI_PROC_NULL || nbr_du[1] != MPI_PROC_NULL) {
        const int width = periodic ? pitch : nx_local;
        if (nz_local == 1) {
            face_type[1] = MPI_DOUBLE;
            face_count[1] = width;
        } else {
            MPI_Type_create_hvector(nz_local, width, plane_bytes, MPI_DOUBLE, &face_type[1]);
            MPI_Type_commit(&face_type[1]);
        }
    }
    if (nbr_fb[0] != MPI_PROC_NULL || nbr_fb[1] != MPI_PROC_NULL) {
        MPI_Type_vector(periodic ? rows : ny_local,
                        periodic ? pitch : nx_local,
                        pitch,
                        MPI_DOUBLE,
                        &face_type[2]);
        MPI_Type_commit(&face_type[2]);
    }
}

void Decomposition::finalize() {
    for (auto& type : face_type) {
        if (type != MPI_DATATYPE_NULL && type != MPI_DOUBLE)
            MPI_Type_free(&type);
    }
    if (cart_comm != MPI_COMM_NULL)
        MPI_Comm_free(&cart_comm);
}
