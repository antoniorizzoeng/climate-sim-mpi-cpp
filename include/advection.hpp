#pragma once
#include "field.hpp"

void advection_step(const Field& u, Field& out, double vx, double vy, double dt);
