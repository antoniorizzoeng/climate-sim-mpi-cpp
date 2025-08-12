#pragma once
#include <mpi.h>
#include "field.hpp"
#include "decomp.hpp"

void exchange_halos(Field& f, const Decomp2D& dec, MPI_Comm comm);
