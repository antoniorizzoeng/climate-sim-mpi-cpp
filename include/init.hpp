#pragma once
#include "decomp.hpp"
#include "field.hpp"
#include "io.hpp"

void apply_initial_condition(const Decomp2D& dec, Field& u, const SimConfig& cfg);

Field initialize_field(MPI_Comm comm, Decomp2D& dec, SimConfig& cfg);
