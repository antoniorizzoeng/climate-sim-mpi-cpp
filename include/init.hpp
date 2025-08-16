#pragma once
#include "decomp.hpp"
#include "field.hpp"
#include "io.hpp"

void apply_initial_condition(const Decomp2D& dec, Field& u, const SimConfig& cfg);

bool ic_from_netcdf_global(const Decomp2D& dec,
                           Field& u,
                           const std::string& path,
                           const std::string& varname);