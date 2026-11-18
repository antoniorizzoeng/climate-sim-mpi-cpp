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
    double yc_frac = 0.5, zc_frac = 0.5;
    bool replicate_z = false;
    std::string path;
    std::string var = "u";
};

struct WindConfig {
    std::string mode = "uniform";
    std::string path;
    std::string vx_var = "vx", vy_var = "vy", vz_var;
    double interval = 3600.0;
};

struct SimConfig {
    int nx = 256, ny = 256, nz = 1;
    bool nx_explicit = false, ny_explicit = false, nz_explicit = false;
    double dx = 1.0, dy = 1.0, dz = 1.0;

    double D = 0.0;
    std::optional<double> Dz;
    double vertical_diffusion() const { return Dz.value_or(D); }
    double vx = 0.0, vy = 0.0, vz = 0.0;

    double dt = 0.1;
    int steps = 100;
    int out_every = 50;

    BCConfig bc;

    std::string output_prefix = "snap";
    std::string output_path = "outputs/snapshots.nc";
    std::optional<std::string> progress_path;

    ICConfig ic{};
    WindConfig wind{};

    void validate() const;
};

struct CLIOverrides {
    std::optional<int> nx, ny, nz;
    std::optional<double> dx, dy, dz;

    std::optional<double> D, Dz, vx, vy, vz;

    std::optional<double> dt;
    std::optional<int> steps, out_every;

    std::optional<BCType> bc_left, bc_right, bc_bottom, bc_top, bc_front, bc_back;

    std::optional<std::string> output_prefix, output_path, progress_path;

    struct {
        std::optional<std::string> mode, preset, path, format, var;
        std::optional<double> A, sigma_frac, xc_frac, yc_frac, zc_frac;
        std::optional<bool> replicate_z;
    } ic;

    struct {
        std::optional<std::string> mode, path, vx_var, vy_var, vz_var;
        std::optional<double> interval;
    } wind;
};

SimConfig load_yaml_file(const std::string& path);

CLIOverrides parse_cli_overrides(const std::vector<std::string>& args);

SimConfig merged_config(const std::optional<std::string>& yaml_path,
                        const std::vector<std::string>& cli_args,
                        std::optional<std::string>* configured_progress = nullptr);

BCType bc_from_string(const std::string& s);
std::string bc_to_string(BCType bc);

int open_netcdf_parallel(const std::string& filename,
                         const Decomposition& dec,
                         const SimConfig& cfg,
                         MPI_Comm comm,
                         int& ncid,
                         int& varid);

bool write_field_netcdf(int ncid, int varid, const Field& f, const Decomposition& dec, int step);

void close_netcdf_parallel(int ncid, MPI_Comm comm = MPI_COMM_NULL);

void write_metadata_netcdf(int ncid, const SimConfig& cfg);
