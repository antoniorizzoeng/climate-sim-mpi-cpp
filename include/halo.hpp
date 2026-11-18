#pragma once
#include <mpi.h>

#include "decomp.hpp"
#include "field.hpp"

void exchange_halos(Field& f, const Decomposition& dec, MPI_Comm comm);
