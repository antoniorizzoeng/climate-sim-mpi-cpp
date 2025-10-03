#include <mpi.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
namespace fs = std::filesystem;

#include "advection.hpp"
#include "boundary.hpp"
#include "decomp.hpp"
#include "diffusion.hpp"
#include "field.hpp"
#include "halo.hpp"
#include "init.hpp"
#include "io.hpp"
#include "stability.hpp"

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int world_rank = 0, world_size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::vector<std::string> args(argv + 1, argv + argc);
    std::optional<std::string> cfg_path;
    for (size_t i = 0; i < args.size(); ++i) {
        const auto& a = args[i];
        if (a.rfind("--config=", 0) == 0)
            cfg_path = a.substr(9);
        else if (a == "--config" && i + 1 < args.size())
            cfg_path = args[i + 1];
    }

    SimConfig cfg = merged_config(cfg_path, args);

    double dt_limit = safe_dt(cfg.dx, cfg.dy, cfg.vx, cfg.vy, cfg.D);
    if (cfg.dt > dt_limit) {
        if (world_rank == 0) {
            std::cerr << "[warn] dt=" << cfg.dt << " exceeds stability limit " << dt_limit
                      << " -> clamping to dt=" << dt_limit << "\n";
        }
        cfg.dt = dt_limit;
    }

    if (world_rank == 0) {
        std::cout << "climate-sim-mpi-cpp \n"
                  << "  grid: " << cfg.nx << " x " << cfg.ny << "  dt: " << cfg.dt
                  << "  steps: " << cfg.steps << "  D: " << cfg.D << "  v=(" << cfg.vx << ","
                  << cfg.vy << ")\n"
                  << "  bc: left=" << bc_to_string(cfg.bc.left)
                  << " right=" << bc_to_string(cfg.bc.right)
                  << " bottom=" << bc_to_string(cfg.bc.bottom)
                  << " top=" << bc_to_string(cfg.bc.top) << "\n";
    }

    Decomp2D dec;
    dec.init(MPI_COMM_WORLD, cfg.nx, cfg.ny);

    const int halo = 1;
    Field u(dec.nx_local, dec.ny_local, halo, cfg.dx, cfg.dy);
    Field tmp(dec.nx_local, dec.ny_local, halo, cfg.dx, cfg.dy);
    u.fill(0.0);
    tmp.fill(0.0);

    apply_initial_condition(dec, u, cfg);

    if (world_rank == 0) {
        double mn = *std::min_element(u.data.begin(), u.data.end());
        double mx = *std::max_element(u.data.begin(), u.data.end());
        std::cout << "IC min/max: " << mn << " / " << mx << "\n";
    }

    if (world_rank == 0) {
        fs::create_directories("outputs");
    }
    MPI_Barrier(MPI_COMM_WORLD);

    int ncid, varid;
    if (world_rank == 0)
        std::cout << "Opening NetCDF file for parallel output\n";
    open_netcdf_parallel("outputs/snapshots.nc", dec, MPI_COMM_WORLD, ncid, varid);

    double t0 = MPI_Wtime();
    double sum_step = 0.0, max_step = 0.0, min_step = 1e300;

    int time_index = 0;
    for (int n = 0; n < cfg.steps; ++n) {
        double ts = MPI_Wtime();

        if (n % cfg.out_every == 0 || n == 0) {
            write_field_netcdf(ncid, varid, u, dec, time_index);
            time_index++;
        }

        exchange_halos(u, dec, MPI_COMM_WORLD);
        apply_boundary(u, dec, cfg.bc, 0.0);

        std::copy(u.data.begin(), u.data.end(), tmp.data.begin());

        diffusion_step(u, tmp, cfg.D, cfg.dt);
        advection_step(u, tmp, cfg.vx, cfg.vy, cfg.dt);

        std::swap(u.data, tmp.data);

        double te = MPI_Wtime();
        double dt = te - ts;
        sum_step += dt;
        if (dt > max_step)
            max_step = dt;
        if (dt < min_step)
            min_step = dt;
    }

    close_netcdf_parallel(ncid);

    double t1 = MPI_Wtime();
    double total = t1 - t0;

    double total_max = 0.0, step_worst = 0.0;
    double avg_step = sum_step / std::max(1, cfg.steps);
    MPI_Reduce(&total, &total_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&avg_step, &step_worst, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (world_rank == 0) {
        std::cout << "timing: total_max=" << total_max << " s, worst_avg_step=" << step_worst
                  << " s\n";
    }

    dec.finalize();
    MPI_Finalize();
    return 0;
}
