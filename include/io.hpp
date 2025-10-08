#pragma once
#include <optional>
#include <string>
#include <vector>

#include "boundary.hpp"
#include "decomp.hpp"
#include "field.hpp"

struct ICConfig {
    std::string mode = "preset";
    std::string preset = "gaussian_hotspot";
    double A = 1.0;
    double sigma_frac = 0.05;
    double xc_frac = 0.5;
    double yc_frac = 0.5;
    std::string path;
    std::string var;
};

struct SimConfig {
    int nx = 256, ny = 256;
    double dx = 1.0, dy = 1.0;

    double D = 0.0;
    double vx = 0.0, vy = 0.0;

    double dt = 0.1;
    int steps = 100;
    int out_every = 50;

    BCConfig bc;

    std::string output_prefix = "snap";

    ICConfig ic{};

    void validate() const;
};

struct CLIOverrides {
    std::optional<int> nx, ny;
    std::optional<double> dx, dy;

    std::optional<double> D, vx, vy;

    std::optional<double> dt;
    std::optional<int> steps, out_every;

    std::optional<BCType> bc_left, bc_right, bc_bottom, bc_top;

    std::optional<std::string> output_prefix;

    struct {
        std::optional<std::string> mode, preset, path, format, var;
        std::optional<double> A, sigma_frac, xc_frac, yc_frac;
    } ic;
};

SimConfig load_yaml_file(const std::string& path);

CLIOverrides parse_cli_overrides(const std::vector<std::string>& args);

SimConfig merged_config(const std::optional<std::string>& yaml_path,
                        const std::vector<std::string>& cli_args);

BCType bc_from_string(const std::string& s);
std::string bc_to_string(BCType bc);

int open_netcdf_parallel(const std::string& filename,
                         const Decomp2D& dec,
                         const SimConfig& cfg,
                         MPI_Comm comm,
                         int& ncid,
                         int& varid);

bool write_field_netcdf(int ncid, int varid, const Field& f, const Decomp2D& dec, int step);

void close_netcdf_parallel(int ncid);

void write_metadata_netcdf(int ncid, const SimConfig& cfg);