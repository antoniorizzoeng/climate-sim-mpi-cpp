#include "io.hpp"

#include <netcdf.h>
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
        if (root["bc"].IsScalar()) {
            cfg.bc = bc_from_string(root["bc"].as<std::string>());
        } else if (root["bc"]["type"]) {
            cfg.bc = bc_from_string(root["bc"]["type"].as<std::string>());
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
        if (ic["format"])
            cfg.ic.format = ic["format"].as<std::string>();
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

        if (starts_with(a, "--bc=") || a == "--bc") {
            std::string val;
            if (auto v = get_value(a, "bc"))
                val = *v;
            else if (a == "--bc" && i + 1 < args.size())
                val = args[i + 1];
            else
                continue;
            o.bc = bc_from_string(val);
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
        if (try_set_str(a, "ic.format", o.ic.format, i))
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

    if (o.bc)
        base.bc = *o.bc;

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
    if (o.ic.format)
        base.ic.format = *o.ic.format;
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

void write_rank_layout_csv(const std::string& path,
                           int rank,
                           int nx_global,
                           int ny_global,
                           int x_offset,
                           int y_offset,
                           int nx_local,
                           int ny_local,
                           int halo) {
    const bool exists = fs::exists(path);
    fs::path p(path);
    if (!p.parent_path().empty())
        fs::create_directories(p.parent_path());
    std::ofstream ofs(path, std::ios::app);
    if (!ofs)
        throw std::runtime_error("Failed to open rank layout file: " + path);
    if (!exists) {
        ofs << "rank,x_offset,y_offset,nx_local,ny_local,halo,nx_global,ny_global\n";
    }
    ofs << rank << "," << x_offset << "," << y_offset << "," << nx_local << "," << ny_local << ","
        << halo << "," << nx_global << "," << ny_global << "\n";
}

static inline void nc_throw_if(int status, const char* where) {
    if (status != NC_NOERR) {
        std::ostringstream oss;
        oss << where << ": " << nc_strerror(status);
        throw std::runtime_error(oss.str());
    }
}

std::string snapshot_filename_nc(const std::string& outdir, int step, int rank) {
    fs::create_directories(outdir);
    char buf[256];
    std::snprintf(buf, sizeof(buf), "snapshot_%05d_rank%05d.nc", step, rank);
    return (fs::path(outdir) / buf).string();
}

bool write_field_netcdf(const Field& f, const std::string& filename, const Decomp2D& /*dec*/) {
    int ncid;
    int status = nc_create(filename.c_str(), NC_NETCDF4 | NC_CLOBBER, &ncid);
    if (status != NC_NOERR)
        return false;

    size_t ny = static_cast<size_t>(f.ny_total());
    size_t nx = static_cast<size_t>(f.nx_total());

    int dim_y, dim_x;
    if (nc_def_dim(ncid, "y", ny, &dim_y) != NC_NOERR) {
        nc_close(ncid);
        return false;
    }
    if (nc_def_dim(ncid, "x", nx, &dim_x) != NC_NOERR) {
        nc_close(ncid);
        return false;
    }

    int dims[2] = {dim_y, dim_x};
    int varid;
    if (nc_def_var(ncid, "u", NC_DOUBLE, 2, dims, &varid) != NC_NOERR) {
        nc_close(ncid);
        return false;
    }

    nc_put_att_text(ncid, varid, "long_name", 1, "u");

    if (nc_enddef(ncid) != NC_NOERR) {
        nc_close(ncid);
        return false;
    }

    std::vector<double> flat(nx * ny);
    for (size_t j = 0; j < ny; ++j) {
        for (size_t i = 0; i < nx; ++i) {
            flat[j * nx + i] = f.at(static_cast<int>(i), static_cast<int>(j));
        }
    }

    if (nc_put_var_double(ncid, varid, flat.data()) != NC_NOERR) {
        nc_close(ncid);
        return false;
    }

    nc_close(ncid);
    return true;
}
