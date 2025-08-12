#pragma once
#include "field.hpp"
#include "decomp.hpp"

enum class BCType { Dirichlet, Neumann, Periodic };

void apply_boundary(Field& f, const Decomp2D& dec, BCType bc, double value = 0.0);
