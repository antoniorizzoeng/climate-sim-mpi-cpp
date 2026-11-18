#pragma once
#include "decomp.hpp"
#include "field.hpp"

enum class BCType { Dirichlet, Neumann, Periodic };

struct BCConfig {
    BCType left = BCType::Dirichlet;
    BCType right = BCType::Dirichlet;
    BCType bottom = BCType::Dirichlet;
    BCType top = BCType::Dirichlet;
    BCType front = BCType::Dirichlet, back = BCType::Dirichlet;
};

void apply_boundary(Field& f, const Decomposition& dec, const BCConfig& bc, double value);
