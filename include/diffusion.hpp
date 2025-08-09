#pragma once
#include "field.hpp"

void diffusion_step(const Field& u, Field& out, double D, double dt);
