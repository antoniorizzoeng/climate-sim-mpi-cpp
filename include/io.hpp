#pragma once
#include "field.hpp"
#include "boundary.hpp"
#include <string>
#include <vector>
#include <optional>

struct SimConfig {
    int nx = 256, ny = 256;
    double dx = 1.0, dy = 1.0;

    double D = 0.0;
    double vx = 0.0, vy = 0.0;

    double dt = 0.1;
    int steps = 100;
    int out_every = 50;

    BCType bc = BCType::Dirichlet;

    std::string output_prefix = "snap";

    void validate() const;
};

SimConfig load_yaml_file(const std::string& path);

SimConfig parse_cli_overrides(const std::vector<std::string>& args);

SimConfig merged_config(const std::optional<std::string>& yaml_path,
                        const std::vector<std::string>& cli_args);

void write_field_csv(const Field& f, const std::string& filename);

BCType bc_from_string(const std::string& s);
std::string bc_to_string(BCType bc);
