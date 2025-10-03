#pragma once
#include "decomp.hpp"
#include "field.hpp"

enum class BCType { Dirichlet, Neumann, Periodic };

struct BCConfig {
    BCType left = BCType::Dirichlet;
    BCType right = BCType::Dirichlet;
    BCType bottom = BCType::Dirichlet;
    BCType top = BCType::Dirichlet;
};

void apply_boundary(Field& f, const Decomp2D& dec, const BCConfig& bc, double value);
