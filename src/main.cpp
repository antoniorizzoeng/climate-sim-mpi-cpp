#include <mpi.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "boundary.hpp"
#include "decomp.hpp"
#include "field.hpp"
#include "init.hpp"
#include "io.hpp"
#include "stability.hpp"
#include "step.hpp"

namespace fs = std::filesystem;

namespace {

void create_parent(const fs::path& path) {
    if (path.has_parent_path())
        fs::create_directories(path.parent_path());
}

std::string json_string(const std::string& value) {
    std::ostringstream json;
    json << '"' << std::hex << std::setfill('0');
    for (unsigned char c : value) {
        if (c == '"' || c == '\\')
            json << '\\' << c;
        else if (c < 0x20)
            json << "\\u" << std::setw(4) << static_cast<int>(c);
        else
            json << c;
    }
    json << '"';
    return json.str();
}

struct RunProgress {
    std::optional<std::string> path;
    int step = 0;
    int steps = 0;
    double dt = 0.0;
    long long frames = 0;

    void write(const char* status, const std::string& message = "") const {
        std::ostringstream json;
        json.imbue(std::locale::classic());
        json << std::setprecision(std::numeric_limits<double>::max_digits10) << "{\"step\":" << step
             << ",\"steps\":" << steps << ",\"time\":" << step * dt << ",\"frames\":" << frames
             << ",\"status\":" << json_string(status) << ",\"message\":" << json_string(message)
             << "}\n";
        const std::string contents = json.str();

        create_parent(*path);
        // A unique file beside the destination keeps rename atomic across filesystem mounts.
        std::string temp = (fs::path(*path).parent_path() / ".climate-progress-XXXXXX").string();
        const int fd = mkstemp(temp.data());
        if (fd == -1)
            throw std::runtime_error("Cannot create progress temporary file for '" + *path + "'");

        auto* file = fdopen(fd, "w");
        if (file == nullptr) {
            close(fd);
            std::remove(temp.c_str());
            throw std::runtime_error("Cannot open progress temporary file for '" + *path + "'");
        }
        const bool written =
            std::fwrite(contents.data(), 1, contents.size(), file) == contents.size();
        const bool closed = std::fclose(file) == 0;
        if (!written || !closed || std::rename(temp.c_str(), path->c_str()) != 0) {
            std::remove(temp.c_str());
            throw std::runtime_error("Cannot publish progress file '" + *path + "'");
        }
    }
};

// Root-only filesystem failures must reach all ranks before the next collective operation.
template <typename Action>
void on_root(int rank, Action action) {
    std::string error;
    if (rank == 0) {
        try {
            action();
        } catch (const std::exception& e) {
            error = e.what();
        }
    }
    int length = static_cast<int>(error.size());
    MPI_Bcast(&length, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (length != 0) {
        error.resize(length);
        MPI_Bcast(error.data(), length, MPI_CHAR, 0, MPI_COMM_WORLD);
        throw std::runtime_error(error);
    }
}

void log_error(std::string message) {
    std::replace(message.begin(), message.end(), '\n', ' ');
    std::replace(message.begin(), message.end(), '\r', ' ');
    std::cerr << "[error] " << message << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    initialize_mpi(argc, argv);

    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    RunProgress progress;
    Decomp2D dec;
    int ncid = -1;
    int result = 0;
    try {
        const std::vector<std::string> args(argv + 1, argv + argc);
        std::optional<std::string> cfg_path;
        for (size_t i = 0; i < args.size(); ++i) {
            const auto& a = args[i];
            if (a.rfind("--config=", 0) == 0)
                cfg_path = a.substr(9);
            else if (a == "--config" && i + 1 < args.size())
                cfg_path = args[i + 1];
        }

        SimConfig cfg = merged_config(cfg_path, args, &progress.path);
        progress.steps = cfg.steps;
        const double dt_limit = safe_dt(cfg.dx, cfg.dy, cfg.vx, cfg.vy, cfg.D);
        if (cfg.dt > dt_limit) {
            if (rank == 0) {
                std::cerr << "[warn] dt=" << cfg.dt << " exceeds stability limit " << dt_limit
                          << " -> clamping to dt=" << dt_limit << "\n";
            }
            cfg.dt = dt_limit;
        }
        if (cfg.dt <= 0 || !std::isfinite(cfg.steps * cfg.dt))
            throw std::runtime_error("Simulation timestep or total time is out of range");
        progress.dt = cfg.dt;

        if (progress.path) {
            on_root(rank, [&] {
                if (fs::weakly_canonical(cfg.output_path) == fs::weakly_canonical(*progress.path))
                    throw std::runtime_error("Snapshot and progress paths must be different");
                if (cfg.ic.mode == "file" &&
                    fs::weakly_canonical(cfg.ic.path) == fs::weakly_canonical(*progress.path)) {
                    progress.path.reset();
                    throw std::runtime_error(
                        "Initial condition and progress paths must be different");
                }
                progress.write("running");
            });
        }

        Field u = initialize_field(MPI_COMM_WORLD, dec, cfg);
        Field tmp(dec.nx_local, dec.ny_local, u.halo, cfg.dx, cfg.dy);
        if (rank == 0) {
            std::cout << "climate-sim-mpi-cpp \n"
                      << "  grid: " << cfg.nx << " x " << cfg.ny << "  dt: " << cfg.dt
                      << "  steps: " << cfg.steps << "  D: " << cfg.D << "  v=(" << cfg.vx << ","
                      << cfg.vy << ")\n"
                      << "  bc: left=" << bc_to_string(cfg.bc.left)
                      << " right=" << bc_to_string(cfg.bc.right)
                      << " bottom=" << bc_to_string(cfg.bc.bottom)
                      << " top=" << bc_to_string(cfg.bc.top) << "\n";
            const auto [min, max] = std::minmax_element(u.data.begin(), u.data.end());
            std::cout << "IC min/max: " << *min << " / " << *max << "\n";
            std::cout << "Opening NetCDF file for parallel output\n";
        }

        on_root(rank, [&] { create_parent(cfg.output_path); });
        int file, varid;
        open_netcdf_parallel(cfg.output_path, dec, cfg, MPI_COMM_WORLD, file, varid);
        ncid = file;

        auto publish = [&](const char* status) {
            if (progress.path)
                on_root(rank, [&] { progress.write(status); });
        };
        auto snapshot = [&] {
            if (!write_field_netcdf(ncid, varid, u, dec, static_cast<int>(progress.frames))) {
                throw std::runtime_error("Cannot write snapshot to '" + cfg.output_path +
                                         "' at step " + std::to_string(progress.step));
            }
            ++progress.frames;
            publish("running");
        };

        const double t0 = MPI_Wtime();
        double sum_step = 0.0;
        while (progress.step < cfg.steps) {
            const double ts = MPI_Wtime();
            if (progress.step % cfg.out_every == 0)
                snapshot();

            advance_field(u, tmp, dec, cfg);
            ++progress.step;
            sum_step += MPI_Wtime() - ts;
        }
        // The loop only writes pre-update states, so the final state is always new.
        snapshot();
        close_netcdf_parallel(std::exchange(ncid, -1), MPI_COMM_WORLD);
        publish("done");

        const double total = MPI_Wtime() - t0;
        const double avg_step = sum_step / cfg.steps;
        double total_max = 0.0, step_worst = 0.0;
        MPI_Reduce(&total, &total_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&avg_step, &step_worst, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

        if (rank == 0) {
            std::cout << "timing: total_max=" << total_max << " s, worst_avg_step=" << step_worst
                      << " s\n";
        }
    } catch (const std::exception& e) {
        result = 1;
        if (rank == 0) {
            log_error(e.what());
            if (progress.path) {
                try {
                    progress.write("error", e.what());
                } catch (const std::exception& progress_error) {
                    log_error(progress_error.what());
                }
            }
        }
        if (ncid != -1) {
            try {
                close_netcdf_parallel(ncid, MPI_COMM_WORLD);
            } catch (const std::exception& close_error) {
                if (rank == 0)
                    log_error(close_error.what());
            }
        }
    }

    dec.finalize();
    MPI_Finalize();
    return result;
}
