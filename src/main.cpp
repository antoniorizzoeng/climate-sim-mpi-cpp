#include <mpi.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
namespace fs = std::filesystem;

#include "boundary.hpp"
#include "decomp.hpp"
#include "field.hpp"
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
    double dt_eff = cfg.dt;
    if (dt_eff > dt_limit) {
        if (world_rank == 0) {
            std::cerr << "[warn] dt=" << cfg.dt << " exceeds stability limit " << dt_limit
                      << " -> clamping to dt=" << dt_limit << "\n";
        }
        dt_eff = dt_limit;
    }
    cfg.dt = dt_eff;

    if (world_rank == 0) {
        std::cout << "climate-sim-mpi-cpp \n"
                  << "  grid: " << cfg.nx << " x " << cfg.ny << "  dt: " << cfg.dt
                  << "  steps: " << cfg.steps << "  D: " << cfg.D << "  v=(" << cfg.vx << ","
                  << cfg.vy << ")"
                  << "  bc=" << bc_to_string(cfg.bc) << "\n";
    }

    Decomp2D dec;
    dec.init(MPI_COMM_WORLD, cfg.nx, cfg.ny);

    const int halo = 1;
    Field u(dec.nx_local, dec.ny_local, halo, cfg.dx, cfg.dy);
    Field tmp(dec.nx_local, dec.ny_local, halo, cfg.dx, cfg.dy);
    u.fill(0.0);
    tmp.fill(0.0);

    if (world_rank == 0) {
        std::filesystem::create_directories("outputs");
        {
            std::ofstream ofs("outputs/rank_layout.csv", std::ios::app);
            if (!ofs)
                throw std::runtime_error("Failed to open outputs/rank_layout.csv");
            if (ofs.tellp() == 0) {
                ofs << "rank,x_offset,y_offset,nx_local,ny_local,halo,nx_global,ny_global\n";
            }
        }
        std::filesystem::create_directories("outputs/snapshots");
    }
    MPI_Barrier(MPI_COMM_WORLD);

    write_rank_layout_csv("outputs/rank_layout.csv",
                          world_rank,
                          cfg.nx,
                          cfg.ny,
                          dec.x_offset,
                          dec.y_offset,
                          dec.nx_local,
                          dec.ny_local,
                          halo);

    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();

    double sum_step = 0.0, max_step = 0.0, min_step = 1e300;
    for (int n = 0; n < cfg.steps; ++n) {
        double ts = MPI_Wtime();

        if (n % cfg.out_every == 0) {
            auto fname = snapshot_filename("outputs/snapshots", n, world_rank);
            write_field_csv(u, fname);
        }
        // exchange_halos(u);
        // apply_boundary(u);
        // compute_diffusion(u, tmp, cfg); // TODO
        // compute_advection(u, tmp, cfg); // TODO
        // std::swap(u.data, tmp.data);

        double te = MPI_Wtime();
        double dt = te - ts;
        sum_step += dt;
        if (dt > max_step)
            max_step = dt;
        if (dt < min_step)
            min_step = dt;
    }

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
