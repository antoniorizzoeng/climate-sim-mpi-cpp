#pragma once
#include "field.hpp"
#include "boundary.hpp"
#include <string>

struct SimConfig {
    int nx = 0, ny = 0;
    double dx = 1.0, dy = 1.0;
    double D = 0.0;
    double vx = 0.0, vy = 0.0;
    double dt = 0.0;
    int steps = 0;
    BCType bc = BCType::Dirichlet;
};

void write_field_csv(const Field& f, const std::string& filename);
