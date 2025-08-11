#include "io.hpp"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <sstream>
#include <cctype>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){return std::tolower(c);});
    return s;
}

BCType bc_from_string(const std::string& s) {
    auto t = lower(s);
    if (t=="dirichlet" || t=="fixed") return BCType::Dirichlet;
    if (t=="neumann" || t=="noflux" || t=="zero-flux") return BCType::Neumann;
    if (t=="periodic" || t=="period") return BCType::Periodic;
    throw std::runtime_error("Unknown BC type: " + s);
}

std::string bc_to_string(BCType bc) {
    switch (bc) {
        case BCType::Dirichlet: return "dirichlet";
        case BCType::Neumann:   return "neumann";
        case BCType::Periodic:  return "periodic";
    }
    return "dirichlet";
}

void SimConfig::validate() const {
    if (nx <= 0 || ny <= 0) throw std::runtime_error("nx/ny must be > 0");
    if (dx <= 0 || dy <= 0) throw std::runtime_error("dx/dy must be > 0");
    if (dt <= 0)            throw std::runtime_error("dt must be > 0");
    if (steps <= 0)         throw std::runtime_error("steps must be > 0");
    if (out_every < 1)      throw std::runtime_error("out_every must be >= 1");
}

static void assign_if(const YAML::Node& n, const char* key, int& x)    { if (n[key]) x = n[key].as<int>(); }
static void assign_if(const YAML::Node& n, const char* key, double& x) { if (n[key]) x = n[key].as<double>(); }
static void assign_if(const YAML::Node& n, const char* key, std::string& x){ if (n[key]) x = n[key].as<std::string>(); }

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
        assign_if(p, "D",  cfg.D);
        assign_if(p, "vx", cfg.vx);
        assign_if(p, "vy", cfg.vy);
    } else {
        assign_if(root, "D",  cfg.D);
        assign_if(root, "vx", cfg.vx);
        assign_if(root, "vy", cfg.vy);
    }

    if (root["time"]) {
        auto t = root["time"];
        assign_if(t, "dt",    cfg.dt);
        assign_if(t, "steps", cfg.steps);
        if (t["out_every"]) cfg.out_every = t["out_every"].as<int>();
    } else {
        assign_if(root, "dt",    cfg.dt);
        assign_if(root, "steps", cfg.steps);
        if (root["out_every"]) cfg.out_every = root["out_every"].as<int>();
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

    cfg.validate();
    return cfg;
}

static bool starts_with(const std::string& s, const std::string& p) {
    return s.rfind(p, 0) == 0;
}

static std::optional<std::string> get_value(const std::string& arg, const std::string& key) {
    if (starts_with(arg, "--" + key + "=")) {
        return arg.substr(key.size()+3);
    }
    return std::nullopt;
}

SimConfig parse_cli_overrides(const std::vector<std::string>& args) {
    SimConfig o;

    for (size_t i=0; i<args.size(); ++i) {
        const std::string& a = args[i];

        auto set_int = [&](const char* k, int& dst) {
            if (auto v = get_value(a, k)) { dst = std::stoi(*v); return true; }
            if (a == std::string("--") + k && i+1 < args.size()) { dst = std::stoi(args[i+1]); return true; }
            return false;
        };
        auto set_dbl = [&](const char* k, double& dst) {
            if (auto v = get_value(a, k)) { dst = std::stod(*v); return true; }
            if (a == std::string("--") + k && i+1 < args.size()) { dst = std::stod(args[i+1]); return true; }
            return false;
        };
        auto set_str = [&](const char* k, std::string& dst) {
            if (auto v = get_value(a, k)) { dst = *v; return true; }
            if (a == std::string("--") + k && i+1 < args.size()) { dst = args[i+1]; return true; }
            return false;
        };

        if (set_int("nx", o.nx)) continue;
        if (set_int("ny", o.ny)) continue;
        if (set_dbl("dx", o.dx)) continue;
        if (set_dbl("dy", o.dy)) continue;

        if (set_dbl("D",  o.D))  continue;
        if (set_dbl("vx", o.vx)) continue;
        if (set_dbl("vy", o.vy)) continue;

        if (set_dbl("dt",    o.dt))    continue;
        if (set_int("steps", o.steps)) continue;
        if (set_int("out_every", o.out_every)) continue;

        if (starts_with(a, "--bc=") || a == "--bc") {
            std::string val;
            if (auto v = get_value(a, "bc")) { val = *v; }
            else if (a == "--bc" && i+1 < args.size()) { val = args[i+1]; }
            else continue;
            o.bc = bc_from_string(val);
            continue;
        }

        if (set_str("output_prefix", o.output_prefix)) continue;
    }

    return o;
}

static void apply_overrides(SimConfig& base, const SimConfig& o) {
    base.nx = o.nx ?: base.nx;
    base.nx = (o.nx != SimConfig{}.nx ? o.nx : base.nx);
    base.ny = (o.ny != SimConfig{}.ny ? o.ny : base.ny);
    base.dx = (o.dx != SimConfig{}.dx ? o.dx : base.dx);
    base.dy = (o.dy != SimConfig{}.dy ? o.dy : base.dy);
    base.D  = (o.D  != SimConfig{}.D  ? o.D  : base.D);
    base.vx = (o.vx != SimConfig{}.vx ? o.vx : base.vx);
    base.vy = (o.vy != SimConfig{}.vy ? o.vy : base.vy);
    base.dt = (o.dt != SimConfig{}.dt ? o.dt : base.dt);
    base.steps = (o.steps != SimConfig{}.steps ? o.steps : base.steps);
    base.out_every = (o.out_every != SimConfig{}.out_every ? o.out_every : base.out_every);
    base.bc = (o.bc != SimConfig{}.bc ? o.bc : base.bc);
    base.output_prefix = (o.output_prefix != SimConfig{}.output_prefix ? o.output_prefix : base.output_prefix);
}

SimConfig merged_config(const std::optional<std::string>& yaml_path,
                        const std::vector<std::string>& cli_args)
{
    SimConfig cfg; 
    if (yaml_path && !yaml_path->empty()) {
        cfg = load_yaml_file(*yaml_path);
    }
    SimConfig cli = parse_cli_overrides(cli_args);
    apply_overrides(cfg, cli);
    cfg.validate();
    return cfg;
}

void write_field_csv(const Field& f, const std::string& filename) {
    std::ofstream ofs(filename);
    for (int j = 0; j < f.ny_total(); ++j) {
        for (int i = 0; i < f.nx_total(); ++i) {
            ofs << f.at(i,j);
            if (i < f.nx_total()-1) ofs << ",";
        }
        ofs << "\n";
    }
}

void write_rank_layout_csv(const std::string& path,
                           int rank, int nx_global, int ny_global,
                           int x_offset, int y_offset, int nx_local, int ny_local, int halo)
{
    const bool exists = fs::exists(path);
    fs::path p(path);
    if (!p.parent_path().empty())
        fs::create_directories(p.parent_path());
    std::ofstream ofs(path, std::ios::app);
    if (!ofs) throw std::runtime_error("Failed to open rank layout file: " + path);
    if (!exists) {
        ofs << "rank,x_offset,y_offset,nx_local,ny_local,halo,nx_global,ny_global\n";
    }
    ofs << rank << "," << x_offset << "," << y_offset << ","
        << nx_local << "," << ny_local << "," << halo << ","
        << nx_global << "," << ny_global << "\n";
}

std::string snapshot_filename(const std::string& outdir, int step, int rank) {
    fs::create_directories(outdir);
    char buf[256];
    std::snprintf(buf, sizeof(buf), "snapshot_%05d_rank%05d.csv", step, rank);
    return (fs::path(outdir) / buf).string();
}
