#include <mpi.h>

#include <iostream>

#include "init.hpp"
#include "wind.hpp"

// Compute-only counterpart of the CLI timing interval, including communication.
int main(int argc, char** argv) {
    initialize_mpi(argc, argv);
    SimConfig cfg = merged_config(std::nullopt, {argv + 1, argv + argc});
    Decomp2D dec;
    Field u = initialize_field(MPI_COMM_WORLD, dec, cfg);
    Field tmp(dec.nx_local, dec.ny_local, u.halo, cfg.dx, cfg.dy);
    std::optional<WindFile> wind;
    if (cfg.wind.mode == "file")
        wind.emplace(dec, cfg);
    MPI_Barrier(dec.cart_comm);
    const double start = MPI_Wtime();
    for (int step = 0; step < cfg.steps; ++step) {
        if (wind)
            advance_field(u, tmp, dec, cfg, wind->sample(step * cfg.dt));
        else
            advance_field(u, tmp, dec, cfg);
    }
    const double elapsed = MPI_Wtime() - start;
    double worst;
    MPI_Reduce(&elapsed, &worst, 1, MPI_DOUBLE, MPI_MAX, 0, dec.cart_comm);
    if (dec.coords[0] == 0 && dec.coords[1] == 0)
        std::cout << "timing: total_max=" << worst << " s\n";
    wind.reset();
    dec.finalize();
    MPI_Finalize();
}
