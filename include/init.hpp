#pragma once
#include "decomp.hpp"
#include "field.hpp"
#include "io.hpp"

void initialize_mpi(int& argc, char**& argv);

void apply_initial_condition(const Decomposition& dec, Field& u, const SimConfig& cfg);

Field initialize_field(MPI_Comm comm, Decomposition& dec, SimConfig& cfg);
