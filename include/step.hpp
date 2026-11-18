#pragma once

#include "io.hpp"

// Advance one step and swap storage. Both fields need the same one-cell halo
// layout and initial physical ghost values. Threaded callers use initialize_mpi
// or provide MPI_THREAD_FUNNELED support for multiple OpenMP threads.
void advance_field(Field& u, Field& scratch, const Decomp2D& dec, const SimConfig& cfg);

struct WindTile {
    std::vector<double> vx, vy;
};

// Row-major tile interiors. Both records use the scalar field's decomposition.
struct WindSample {
    const WindTile& lower;
    const WindTile& upper;
    double fraction;
};

void advance_field(
    Field& u, Field& scratch, const Decomp2D& dec, const SimConfig& cfg, WindSample wind);
