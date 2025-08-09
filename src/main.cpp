#include <mpi.h>
#include <iostream>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        std::cout << "climate-sim-mpi-cpp: placeholder build verified with " 
                  << size << " ranks.\n";
    }

    MPI_Finalize();
    return 0;
}
