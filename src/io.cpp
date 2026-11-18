#include "io.hpp"

#include <mpi.h>
#include <pnetcdf.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

#include "boundary.hpp"
#include "field.hpp"

namespace {
inline void ncmpi_check(int status, const char* where) {
    if (status != NC_NOERR) {
        std::ostringstream oss;
        oss << where << ": " << ncmpi_strerror(status);
        throw std::runtime_error(oss.str());
    }
}
int collective_status(MPI_Comm comm, int status) {
    if (comm == MPI_COMM_NULL)
        return status;
    int result;
    MPI_Allreduce(&status, &result, 1, MPI_INT, MPI_MIN, comm);
    return result;
}
}  // namespace

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

BCType bc_from_string(const std::string& s) {
    auto t = lower(s);
    if (t == "dirichlet" || t == "fixed")
        return BCType::Dirichlet;
    if (t == "neumann" || t == "noflux" || t == "zero-flux")
        return BCType::Neumann;
    if (t == "periodic" || t == "period")
        return BCType::Periodic;
    throw std::runtime_error("Unknown BC type: " + s);
}

std::string bc_to_string(BCType bc) {
    switch (bc) {
        case BCType::Dirichlet:
            return "dirichlet";
        case BCType::Neumann:
            return "neumann";
        case BCType::Periodic:
            return "periodic";
    }
    return "dirichlet";
}

void SimConfig::validate() const {
    if (backend != "cpu" && backend != "kokkos")
        throw std::runtime_error("--backend must be cpu or kokkos");
#ifndef CLIMATE_ENABLE_KOKKOS
    if (backend == "kokkos")
        throw std::runtime_error("--backend=kokkos requires a build with ENABLE_KOKKOS=ON");
#endif
    if ((bc.left == BCType::Periodic) != (bc.right == BCType::Periodic))
        throw std::runtime_error("Both left and right boundaries must be periodic to wrap x");
    if ((bc.bottom == BCType::Periodic) != (bc.top == BCType::Periodic))
        throw std::runtime_error("Both bottom and top boundaries must be periodic to wrap y");
    if ((bc.front == BCType::Periodic) != (bc.back == BCType::Periodic))
        throw std::runtime_error("Both front and back boundaries must be periodic to wrap z");
    if (nz < 1 || !std::isfinite(dz) || dz <= 0)
        throw std::runtime_error("nz and dz must be positive and finite");
    if ((Dz && (!std::isfinite(*Dz) || *Dz < 0)) || !std::isfinite(vz))
        throw std::runtime_error("Expected finite vz and nonnegative finite Dz");
    if (nz > 1 && (!std::isfinite(D) || D < 0 || !std::isfinite(dx) || !std::isfinite(dy) ||
                   !std::isfinite(vx) || !std::isfinite(vy)))
        throw std::runtime_error(
            "3D physics requires finite grid spacing and velocities and nonnegative finite D");
    if (wind.mode != "uniform" && wind.mode != "file")
        throw std::runtime_error("wind.mode must be uniform or file");
    if (wind.mode == "file") {
        if (wind.path.empty() || wind.vx_var.empty() || wind.vy_var.empty())
            throw std::runtime_error("File wind requires wind.path, wind.vx_var and wind.vy_var");
        if (!std::isfinite(wind.interval) || wind.interval <= 0)
            throw std::runtime_error("wind.interval must be finite and positive");
        if (!std::isfinite(D) || D < 0 || !std::isfinite(dx) || !std::isfinite(dy))
            throw std::runtime_error(
                "File wind requires finite grid spacing and nonnegative finite D");
    }
    if (nx <= 0 || ny <= 0)
        throw std::runtime_error("nx/ny must be > 0");
    const int max_extent = std::numeric_limits<int>::max() - 2;
    if (nx > max_extent || ny > max_extent || nz > max_extent)
        throw std::runtime_error("Grid dimensions exceed the halo-padded index range");
    if (dx <= 0 || dy <= 0)
        throw std::runtime_error("dx/dy must be > 0");
    if (!std::isfinite(dt) || dt <= 0)
        throw std::runtime_error("dt must be > 0");
    if (steps <= 0)
        throw std::runtime_error("steps must be > 0");
    if (out_every < 1)
        throw std::runtime_error("out_every must be >= 1");
    if (output_path.empty())
        throw std::runtime_error("output.path must not be empty");
    if (progress_path && progress_path->empty())
        throw std::runtime_error("output.progress must not be empty");
}

static void assign_if(const YAML::Node& n, const char* key, int& x) {
    if (n[key])
        x = n[key].as<int>();
}
static void assign_if(const YAML::Node& n, const char* key, double& x) {
    if (n[key])
        x = n[key].as<double>();
}
static void assign_if(const YAML::Node& n, const char* key, std::string& x) {
    if (n[key])
        x = n[key].as<std::string>();
}

static SimConfig read_yaml_file(const std::string& path,
                                std::optional<std::string>* configured_progress) {
    SimConfig cfg;
    YAML::Node root = YAML::LoadFile(path);

    if (const auto output = root["output"]) {
        if (output["progress"]) {
            cfg.progress_path = output["progress"].as<std::string>();
            if (configured_progress && !*configured_progress)
                *configured_progress = cfg.progress_path;
        }
        assign_if(output, "path", cfg.output_path);
        assign_if(output, "prefix", cfg.output_prefix);
    } else {
        assign_if(root, "output_prefix", cfg.output_prefix);
    }

    const auto grid = root["grid"] ? root["grid"] : root;
    cfg.nx_explicit = grid["nx"].IsDefined();
    cfg.ny_explicit = grid["ny"].IsDefined();
    cfg.nz_explicit = grid["nz"].IsDefined();
    assign_if(grid, "nz", cfg.nz);
    assign_if(grid, "dz", cfg.dz);
    assign_if(grid, "nx", cfg.nx);
    assign_if(grid, "ny", cfg.ny);
    assign_if(grid, "dx", cfg.dx);
    assign_if(grid, "dy", cfg.dy);

    const auto physics = root["physics"] ? root["physics"] : root;
    assign_if(physics, "D", cfg.D);
    if (physics["Dz"])
        cfg.Dz = physics["Dz"].as<double>();
    assign_if(physics, "vx", cfg.vx);
    assign_if(physics, "vy", cfg.vy);
    assign_if(physics, "vz", cfg.vz);

    if (root["time"]) {
        auto t = root["time"];
        assign_if(t, "dt", cfg.dt);
        assign_if(t, "steps", cfg.steps);
        if (t["out_every"])
            cfg.out_every = t["out_every"].as<int>();
    } else {
        assign_if(root, "dt", cfg.dt);
        assign_if(root, "steps", cfg.steps);
        if (root["out_every"])
            cfg.out_every = root["out_every"].as<int>();
    }

    if (root["bc"]) {
        auto bcnode = root["bc"];
        if (bcnode.IsScalar()) {
            auto b = bc_from_string(bcnode.as<std::string>());
            cfg.bc.left = cfg.bc.right = cfg.bc.bottom = cfg.bc.top = cfg.bc.front = cfg.bc.back =
                b;
        } else {
            if (bcnode["front"])
                cfg.bc.front = bc_from_string(bcnode["front"].as<std::string>());
            if (bcnode["back"])
                cfg.bc.back = bc_from_string(bcnode["back"].as<std::string>());
            if (bcnode["left"])
                cfg.bc.left = bc_from_string(bcnode["left"].as<std::string>());
            if (bcnode["right"])
                cfg.bc.right = bc_from_string(bcnode["right"].as<std::string>());
            if (bcnode["bottom"])
                cfg.bc.bottom = bc_from_string(bcnode["bottom"].as<std::string>());
            if (bcnode["top"])
                cfg.bc.top = bc_from_string(bcnode["top"].as<std::string>());
        }
    }

    if (auto wind = root["wind"]) {
        assign_if(wind, "mode", cfg.wind.mode);
        assign_if(wind, "path", cfg.wind.path);
        assign_if(wind, "vx_var", cfg.wind.vx_var);
        assign_if(wind, "vy_var", cfg.wind.vy_var);
        assign_if(wind, "vz_var", cfg.wind.vz_var);
        assign_if(wind, "interval", cfg.wind.interval);
    }

    if (root["ic"]) {
        auto ic = root["ic"];
        assign_if(ic, "zc_frac", cfg.ic.zc_frac);
        if (ic["replicate_z"])
            cfg.ic.replicate_z = ic["replicate_z"].as<bool>();
        if (ic["mode"])
            cfg.ic.mode = ic["mode"].as<std::string>();
        if (ic["preset"])
            cfg.ic.preset = ic["preset"].as<std::string>();
        if (ic["A"])
            cfg.ic.A = ic["A"].as<double>();
        if (ic["sigma_frac"])
            cfg.ic.sigma_frac = ic["sigma_frac"].as<double>();
        if (ic["xc_frac"])
            cfg.ic.xc_frac = ic["xc_frac"].as<double>();
        if (ic["yc_frac"])
            cfg.ic.yc_frac = ic["yc_frac"].as<double>();
        if (ic["path"])
            cfg.ic.path = ic["path"].as<std::string>();
        if (ic["var"])
            cfg.ic.var = ic["var"].as<std::string>();
    }

    return cfg;
}

SimConfig load_yaml_file(const std::string& path) {
    SimConfig cfg = read_yaml_file(path, nullptr);
    cfg.validate();
    return cfg;
}

static bool starts_with(const std::string& s, const std::string& p) { return s.rfind(p, 0) == 0; }
static std::optional<std::string> get_value(const std::string& arg, const std::string& key) {
    if (starts_with(arg, "--" + key + "="))
        return arg.substr(key.size() + 3);
    return std::nullopt;
}

static std::optional<std::string> cli_string(const std::vector<std::string>& args,
                                             const char* key,
                                             size_t i) {
    if (auto value = get_value(args[i], key))
        return value;
    if (args[i] == std::string("--") + key && i + 1 < args.size())
        return args[i + 1];
    return std::nullopt;
}

CLIOverrides parse_cli_overrides(const std::vector<std::string>& args) {
    CLIOverrides o;

    auto try_set_int = [&](const std::string& a, const char* k, std::optional<int>& dst, size_t i) {
        if (auto v = get_value(a, k)) {
            dst = std::stoi(*v);
            return true;
        }
        if (a == std::string("--") + k && i + 1 < args.size()) {
            dst = std::stoi(args[i + 1]);
            return true;
        }
        return false;
    };
    auto try_set_dbl =
        [&](const std::string& a, const char* k, std::optional<double>& dst, size_t i) {
            if (auto v = get_value(a, k)) {
                dst = std::stod(*v);
                return true;
            }
            if (a == std::string("--") + k && i + 1 < args.size()) {
                dst = std::stod(args[i + 1]);
                return true;
            }
            return false;
        };
    auto try_set_str = [&](const char* k, std::optional<std::string>& dst, size_t i) {
        if (auto value = cli_string(args, k, i)) {
            dst = std::move(value);
            return true;
        }
        return false;
    };

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];

        if (try_set_int(a, "nz", o.nz, i) || try_set_dbl(a, "dz", o.dz, i) ||
            try_set_dbl(a, "Dz", o.Dz, i) || try_set_dbl(a, "vz", o.vz, i))
            continue;
        if (try_set_int(a, "nx", o.nx, i))
            continue;
        if (try_set_int(a, "ny", o.ny, i))
            continue;
        if (try_set_dbl(a, "dx", o.dx, i))
            continue;
        if (try_set_dbl(a, "dy", o.dy, i))
            continue;

        if (try_set_dbl(a, "D", o.D, i))
            continue;
        if (try_set_dbl(a, "vx", o.vx, i))
            continue;
        if (try_set_dbl(a, "vy", o.vy, i))
            continue;

        if (try_set_dbl(a, "dt", o.dt, i))
            continue;
        if (try_set_int(a, "steps", o.steps, i))
            continue;
        if (try_set_int(a, "out_every", o.out_every, i))
            continue;

        if (auto value = cli_string(args, "bc", i)) {
            o.bc_left = o.bc_right = o.bc_bottom = o.bc_top = o.bc_front = o.bc_back =
                bc_from_string(*value);
            continue;
        }
        auto set_face = [&](const char* key, std::optional<BCType>& face) {
            if (auto value = cli_string(args, key, i); value && !value->empty())
                face = bc_from_string(*value);
        };
        set_face("bc.left", o.bc_left);
        set_face("bc.right", o.bc_right);
        set_face("bc.bottom", o.bc_bottom);
        set_face("bc.top", o.bc_top);
        set_face("bc.front", o.bc_front);
        set_face("bc.back", o.bc_back);
        if (try_set_str("backend", o.backend, i))
            continue;
        if (a == "--backend")
            throw std::runtime_error("--backend requires cpu or kokkos");
        if (try_set_str("output", o.output_path, i))
            continue;
        if (try_set_str("progress", o.progress_path, i))
            continue;
        if (try_set_str("output.prefix", o.output_prefix, i))
            continue;
        if (try_set_str("output_prefix", o.output_prefix, i))
            continue;

        if (try_set_str("wind.mode", o.wind.mode, i) || try_set_str("wind.path", o.wind.path, i) ||
            try_set_str("wind.vx_var", o.wind.vx_var, i) ||
            try_set_str("wind.vz_var", o.wind.vz_var, i) ||
            try_set_str("wind.vy_var", o.wind.vy_var, i) ||
            try_set_dbl(a, "wind.interval", o.wind.interval, i))
            continue;

        if (auto value = cli_string(args, "ic.replicate_z", i))
            o.ic.replicate_z = YAML::Node(*value).as<bool>();
        if (try_set_dbl(a, "ic.zc_frac", o.ic.zc_frac, i))
            continue;
        if (try_set_str("ic.mode", o.ic.mode, i))
            continue;
        if (try_set_str("ic.preset", o.ic.preset, i))
            continue;
        if (try_set_dbl(a, "ic.A", o.ic.A, i))
            continue;
        if (try_set_dbl(a, "ic.sigma_frac", o.ic.sigma_frac, i))
            continue;
        if (try_set_dbl(a, "ic.xc_frac", o.ic.xc_frac, i))
            continue;
        if (try_set_dbl(a, "ic.yc_frac", o.ic.yc_frac, i))
            continue;
        if (try_set_str("ic.path", o.ic.path, i))
            continue;
        if (try_set_str("ic.var", o.ic.var, i))
            continue;
    }
    return o;
}

static void apply_overrides(SimConfig& base, const CLIOverrides& o) {
    if (o.backend)
        base.backend = *o.backend;
    if (o.nz) {
        base.nz = *o.nz;
        base.nz_explicit = true;
    }
    if (o.dz)
        base.dz = *o.dz;
    if (o.Dz)
        base.Dz = o.Dz;
    if (o.vz)
        base.vz = *o.vz;
    if (o.bc_front)
        base.bc.front = *o.bc_front;
    if (o.bc_back)
        base.bc.back = *o.bc_back;
    if (o.wind.vz_var)
        base.wind.vz_var = *o.wind.vz_var;
    if (o.ic.replicate_z)
        base.ic.replicate_z = *o.ic.replicate_z;
    if (o.ic.zc_frac)
        base.ic.zc_frac = *o.ic.zc_frac;
    if (o.nx) {
        base.nx = *o.nx;
        base.nx_explicit = true;
    }
    if (o.ny) {
        base.ny = *o.ny;
        base.ny_explicit = true;
    }
    if (o.dx)
        base.dx = *o.dx;
    if (o.dy)
        base.dy = *o.dy;

    if (o.D)
        base.D = *o.D;
    if (o.vx)
        base.vx = *o.vx;
    if (o.vy)
        base.vy = *o.vy;

    if (o.dt)
        base.dt = *o.dt;
    if (o.steps)
        base.steps = *o.steps;
    if (o.out_every)
        base.out_every = *o.out_every;

    if (o.bc_left)
        base.bc.left = *o.bc_left;
    if (o.bc_right)
        base.bc.right = *o.bc_right;
    if (o.bc_bottom)
        base.bc.bottom = *o.bc_bottom;
    if (o.bc_top)
        base.bc.top = *o.bc_top;

    if (o.output_prefix)
        base.output_prefix = *o.output_prefix;
    if (o.output_path)
        base.output_path = *o.output_path;
    if (o.progress_path)
        base.progress_path = o.progress_path;

    if (o.wind.mode)
        base.wind.mode = *o.wind.mode;
    if (o.wind.path)
        base.wind.path = *o.wind.path;
    if (o.wind.vx_var)
        base.wind.vx_var = *o.wind.vx_var;
    if (o.wind.vy_var)
        base.wind.vy_var = *o.wind.vy_var;
    if (o.wind.interval)
        base.wind.interval = *o.wind.interval;

    if (o.ic.mode)
        base.ic.mode = *o.ic.mode;
    if (o.ic.preset)
        base.ic.preset = *o.ic.preset;
    if (o.ic.A)
        base.ic.A = *o.ic.A;
    if (o.ic.sigma_frac)
        base.ic.sigma_frac = *o.ic.sigma_frac;
    if (o.ic.xc_frac)
        base.ic.xc_frac = *o.ic.xc_frac;
    if (o.ic.yc_frac)
        base.ic.yc_frac = *o.ic.yc_frac;
    if (o.ic.path)
        base.ic.path = *o.ic.path;
    if (o.ic.var)
        base.ic.var = *o.ic.var;
}

SimConfig merged_config(const std::optional<std::string>& yaml_path,
                        const std::vector<std::string>& cli_args,
                        std::optional<std::string>* configured_progress) {
    // Capture the destination before any unrelated option can fail to parse.
    if (configured_progress) {
        configured_progress->reset();
        for (size_t i = 0; i < cli_args.size(); ++i) {
            if (auto path = cli_string(cli_args, "progress", i))
                *configured_progress = std::move(path);
        }
    }
    SimConfig cfg;

    if (yaml_path && !yaml_path->empty()) {
        cfg = read_yaml_file(*yaml_path, configured_progress);
    }

    CLIOverrides cli = parse_cli_overrides(cli_args);
    apply_overrides(cfg, cli);

    cfg.validate();
    return cfg;
}

int open_netcdf_parallel(const std::string& filename,
                         const Decomposition& dec,
                         const SimConfig& cfg,
                         MPI_Comm comm,
                         int& ncid,
                         int& varid) {
    int dim_time, dim_y, dim_x, dim_z;
    const bool volume = dec.nz_global > 1;
    int status =
        ncmpi_create(comm, filename.c_str(), NC_CLOBBER | NC_64BIT_DATA, MPI_INFO_NULL, &ncid);
    ncmpi_check(collective_status(comm, status), "ncmpi_create");

    ncmpi_check(ncmpi_def_dim(ncid, "time", NC_UNLIMITED, &dim_time), "def_dim time");
    if (volume)
        ncmpi_check(ncmpi_def_dim(ncid, "z", dec.nz_global, &dim_z), "def_dim z");
    ncmpi_check(ncmpi_def_dim(ncid, "y", dec.ny_global, &dim_y), "def_dim y");
    ncmpi_check(ncmpi_def_dim(ncid, "x", dec.nx_global, &dim_x), "def_dim x");

    int dims[] = {dim_time, volume ? dim_z : dim_time, dim_y, dim_x};
    ncmpi_check(
        ncmpi_def_var(ncid, "u", NC_DOUBLE, volume ? 4 : 3, dims + (volume ? 0 : 1), &varid),
        "def_var u");

    write_metadata_netcdf(ncid, cfg);

    ncmpi_check(collective_status(comm, ncmpi_enddef(ncid)), "enddef");
    return NC_NOERR;
}

bool write_field_netcdf(int ncid, int varid, const Field& f, const Decomposition& dec, int step) {
    const bool volume = dec.nz_global > 1;
    const int offset = volume ? 0 : 1;
    const MPI_Offset start[] = {step, volume ? dec.z_offset : step, dec.y_offset, dec.x_offset};
    const MPI_Offset count[] = {1, volume ? dec.nz_local : 1, dec.ny_local, dec.nx_local};
    const MPI_Offset imap[] = {
        0, volume ? static_cast<MPI_Offset>(f.plane_size()) : 0, f.nx_total(), 1};
    const double* interior = f.data.data();
    if (dec.nx_local && dec.ny_local && dec.nz_local)
        interior = &f.at(f.halo, f.halo, f.z_halo);
    const int status = ncmpi_put_varm_double_all(
        ncid, varid, start + offset, count + offset, nullptr, imap + offset, interior);
    return collective_status(dec.io_comm, status) == NC_NOERR;
}

void close_netcdf_parallel(int ncid, MPI_Comm comm) {
    ncmpi_check(collective_status(comm, ncmpi_close(ncid)), "ncmpi_close");
}

void write_metadata_netcdf(int ncid, const SimConfig& cfg) {
    auto put_attr = [&](const char* name, const std::string& value) {
        ncmpi_check(ncmpi_put_att_text(ncid, NC_GLOBAL, name, value.size(), value.c_str()), name);
    };

    auto precise = [](double value) {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
        return out.str();
    };

    put_attr("description", "climate-sim-mpi-cpp");
    const bool volume = cfg.nz > 1;
    put_attr("grid",
             std::to_string(cfg.nx) + " x " + std::to_string(cfg.ny) +
                 (volume ? " x " + std::to_string(cfg.nz) : ""));
    if (volume) {
        put_attr("nz", std::to_string(cfg.nz));
        put_attr("dz", precise(cfg.dz));
        put_attr("Dz", precise(cfg.vertical_diffusion()));
    }
    put_attr("dt", precise(cfg.dt));
    put_attr("out_every", std::to_string(cfg.out_every));
    put_attr("output_prefix", cfg.output_prefix);
    put_attr("steps", std::to_string(cfg.steps));
    put_attr("D", std::to_string(cfg.D));
    put_attr("wind_mode", cfg.wind.mode);
    if (cfg.wind.mode == "file") {
        put_attr("wind_path", cfg.wind.path);
        put_attr("wind_vx_var", cfg.wind.vx_var);
        put_attr("wind_vy_var", cfg.wind.vy_var);
        if (volume && !cfg.wind.vz_var.empty())
            put_attr("wind_vz_var", cfg.wind.vz_var);
        put_attr("wind_interval", precise(cfg.wind.interval));
        put_attr("velocity", "file");
    } else
        put_attr("velocity",
                 "(" + std::to_string(cfg.vx) + "," + std::to_string(cfg.vy) +
                     (volume ? "," + std::to_string(cfg.vz) : "") + ")");
    put_attr(
        "boundary_conditions",
        "left=" + bc_to_string(cfg.bc.left) + " right=" + bc_to_string(cfg.bc.right) +
            " bottom=" + bc_to_string(cfg.bc.bottom) + " top=" + bc_to_string(cfg.bc.top) +
            (volume ? " front=" + bc_to_string(cfg.bc.front) + " back=" + bc_to_string(cfg.bc.back)
                    : ""));
}
