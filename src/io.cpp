#include "io.hpp"

#include <mpi.h>
#include <pnetcdf.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "boundary.hpp"
#include "field.hpp"
namespace fs = std::filesystem;

namespace {
inline void ncmpi_check(int status, const char* where) {
    if (status != NC_NOERR) {
        std::ostringstream oss;
        oss << where << ": " << ncmpi_strerror(status);
        throw std::runtime_error(oss.str());
    }
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
    if (nx <= 0 || ny <= 0)
        throw std::runtime_error("nx/ny must be > 0");
    if (dx <= 0 || dy <= 0)
        throw std::runtime_error("dx/dy must be > 0");
    if (dt <= 0)
        throw std::runtime_error("dt must be > 0");
    if (steps <= 0)
        throw std::runtime_error("steps must be > 0");
    if (out_every < 1)
        throw std::runtime_error("out_every must be >= 1");
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

SimConfig load_yaml_file(const std::string& path) {
    SimConfig cfg;
    YAML::Node root = YAML::LoadFile(path);

    if (root["grid"]) {
        auto g = root["grid"];
        assign_if(g, "nx", cfg.nx);
        assign_if(g, "ny", cfg.ny);
        assign_if(g, "dx", cfg.dx);
        assign_if(g, "dy", cfg.dy);
    } else {
        assign_if(root, "nx", cfg.nx);
        assign_if(root, "ny", cfg.ny);
        assign_if(root, "dx", cfg.dx);
        assign_if(root, "dy", cfg.dy);
    }

    if (root["physics"]) {
        auto p = root["physics"];
        assign_if(p, "D", cfg.D);
        assign_if(p, "vx", cfg.vx);
        assign_if(p, "vy", cfg.vy);
    } else {
        assign_if(root, "D", cfg.D);
        assign_if(root, "vx", cfg.vx);
        assign_if(root, "vy", cfg.vy);
    }

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
            cfg.bc.left = cfg.bc.right = cfg.bc.bottom = cfg.bc.top = b;
        } else {
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

    if (root["output"]) {
        auto o = root["output"];
        assign_if(o, "prefix", cfg.output_prefix);
    } else {
        assign_if(root, "output_prefix", cfg.output_prefix);
    }

    if (root["ic"]) {
        auto ic = root["ic"];
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

    cfg.validate();
    return cfg;
}

static bool starts_with(const std::string& s, const std::string& p) { return s.rfind(p, 0) == 0; }
static std::optional<std::string> get_value(const std::string& arg, const std::string& key) {
    if (starts_with(arg, "--" + key + "="))
        return arg.substr(key.size() + 3);
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
    auto try_set_str =
        [&](const std::string& a, const char* k, std::optional<std::string>& dst, size_t i) {
            if (auto v = get_value(a, k)) {
                dst = *v;
                return true;
            }
            if (a == std::string("--") + k && i + 1 < args.size()) {
                dst = args[i + 1];
                return true;
            }
            return false;
        };

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];

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

        if (starts_with(a, "--bc.left=") || a == "--bc.left") {
            std::string val;
            if (auto v = get_value(a, "bc.left"))
                val = *v;
            else if (a == "--bc.left" && i + 1 < args.size())
                val = args[i + 1];
            if (!val.empty())
                o.bc_left = bc_from_string(val);
            continue;
        }
        if (starts_with(a, "--bc.right=") || a == "--bc.right") {
            std::string val;
            if (auto v = get_value(a, "bc.right"))
                val = *v;
            else if (a == "--bc.right" && i + 1 < args.size())
                val = args[i + 1];
            if (!val.empty())
                o.bc_right = bc_from_string(val);
            continue;
        }
        if (starts_with(a, "--bc.bottom=") || a == "--bc.bottom") {
            std::string val;
            if (auto v = get_value(a, "bc.bottom"))
                val = *v;
            else if (a == "--bc.bottom" && i + 1 < args.size())
                val = args[i + 1];
            if (!val.empty())
                o.bc_bottom = bc_from_string(val);
            continue;
        }
        if (starts_with(a, "--bc.top=") || a == "--bc.top") {
            std::string val;
            if (auto v = get_value(a, "bc.top"))
                val = *v;
            else if (a == "--bc.top" && i + 1 < args.size())
                val = args[i + 1];
            if (!val.empty())
                o.bc_top = bc_from_string(val);
            continue;
        }

        if (try_set_str(a, "output.prefix", o.output_prefix, i))
            continue;
        if (try_set_str(a, "output_prefix", o.output_prefix, i))
            continue;

        if (try_set_str(a, "ic.mode", o.ic.mode, i))
            continue;
        if (try_set_str(a, "ic.preset", o.ic.preset, i))
            continue;
        if (try_set_dbl(a, "ic.A", o.ic.A, i))
            continue;
        if (try_set_dbl(a, "ic.sigma_frac", o.ic.sigma_frac, i))
            continue;
        if (try_set_dbl(a, "ic.xc_frac", o.ic.xc_frac, i))
            continue;
        if (try_set_dbl(a, "ic.yc_frac", o.ic.yc_frac, i))
            continue;
        if (try_set_str(a, "ic.path", o.ic.path, i))
            continue;
        if (try_set_str(a, "ic.var", o.ic.var, i))
            continue;
    }
    return o;
}

static void apply_overrides(SimConfig& base, const CLIOverrides& o) {
    if (o.nx)
        base.nx = *o.nx;
    if (o.ny)
        base.ny = *o.ny;
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
}

SimConfig merged_config(const std::optional<std::string>& yaml_path,
                        const std::vector<std::string>& cli_args) {
    SimConfig cfg;

    if (yaml_path && !yaml_path->empty()) {
        cfg = load_yaml_file(*yaml_path);
    }

    CLIOverrides cli = parse_cli_overrides(cli_args);
    apply_overrides(cfg, cli);

    cfg.validate();
    return cfg;
}

int open_netcdf_parallel(
    const std::string& filename, const Decomp2D& dec, MPI_Comm comm, int& ncid, int& varid) {
    int dim_time, dim_y, dim_x;
    int status =
        ncmpi_create(comm, filename.c_str(), NC_CLOBBER | NC_64BIT_DATA, MPI_INFO_NULL, &ncid);
    ncmpi_check(status, "ncmpi_create");

    ncmpi_check(ncmpi_def_dim(ncid, "time", NC_UNLIMITED, &dim_time), "def_dim time");
    ncmpi_check(ncmpi_def_dim(ncid, "y", dec.ny_global, &dim_y), "def_dim y");
    ncmpi_check(ncmpi_def_dim(ncid, "x", dec.nx_global, &dim_x), "def_dim x");

    int dims[3] = {dim_time, dim_y, dim_x};
    ncmpi_check(ncmpi_def_var(ncid, "u", NC_DOUBLE, 3, dims, &varid), "def_var u");

    ncmpi_check(ncmpi_enddef(ncid), "enddef");
    return NC_NOERR;
}

bool write_field_netcdf(int ncid, int varid, const Field& f, const Decomp2D& dec, int step) {
    // hyperslab: [time, y, x]
    MPI_Offset start[3], count[3];
    start[0] = step;
    start[1] = dec.y_offset;
    start[2] = dec.x_offset;
    count[0] = 1;
    count[1] = dec.ny_local;
    count[2] = dec.nx_local;

    std::vector<double> buf((size_t)dec.nx_local * dec.ny_local);
    for (int j = 0; j < dec.ny_local; ++j) {
        for (int i = 0; i < dec.nx_local; ++i) {
            buf[(size_t)j * dec.nx_local + i] = f.at(i + f.halo, j + f.halo);
        }
    }

    int status = ncmpi_put_vara_double_all(ncid, varid, start, count, buf.data());
    if (status != NC_NOERR) {
        std::cerr << "Rank write failed: " << ncmpi_strerror(status) << "\n";
        return false;
    }
    return true;
}

void close_netcdf_parallel(int ncid) { ncmpi_close(ncid); }

void write_metadata_netcdf(int ncid, const SimConfig& cfg) {
    int status;

    auto put_attr = [&](const char* name, const std::string& value) {
        status = ncmpi_put_att_text(ncid, NC_GLOBAL, name, value.size(), value.c_str());
        if (status != NC_NOERR) {
            std::cerr << "Error writing attribute " << name << ": " << ncmpi_strerror(status)
                      << "\n";
        }
    };

    put_attr("description", "climate-sim-mpi-cpp");
    put_attr("grid", std::to_string(cfg.nx) + " x " + std::to_string(cfg.ny));
    put_attr("dt", std::to_string(cfg.dt));
    put_attr("steps", std::to_string(cfg.steps));
    put_attr("D", std::to_string(cfg.D));
    put_attr("velocity", "(" + std::to_string(cfg.vx) + "," + std::to_string(cfg.vy) + ")");
    put_attr("boundary_conditions",
             "left=" + bc_to_string(cfg.bc.left) + " right=" + bc_to_string(cfg.bc.right) +
                 " bottom=" + bc_to_string(cfg.bc.bottom) + " top=" + bc_to_string(cfg.bc.top));
}
