#pragma once
#include "field.hpp"

enum class BCType { Dirichlet, Neumann, Periodic };

void apply_boundary(Field& f, BCType bc, double value = 0.0);
