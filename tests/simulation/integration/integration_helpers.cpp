#include "integration_helpers.hpp"

int run_cmd(const std::string& cmd) { return std::system(cmd.c_str()); }

std::vector<double> read_bin_plane(const fs::path& p, int nx, int ny) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs.good())
        throw std::runtime_error("Failed to open " + p.string());
    std::vector<double> buf((size_t)nx * ny);
    ifs.read(reinterpret_cast<char*>(buf.data()), (std::streamsize)buf.size() * sizeof(double));
    if (!ifs)
        throw std::runtime_error("Short read on " + p.string());
    return buf;
}

std::vector<std::vector<double>> read_nc_2d(const fs::path& p, const char* var) {
    int ncid;
    if (nc_open(p.c_str(), NC_NOWRITE, &ncid) != NC_NOERR)
        throw std::runtime_error("nc_open failed for " + p.string());

    int varid;
    if (nc_inq_varid(ncid, var, &varid) != NC_NOERR) {
        nc_close(ncid);
        throw std::runtime_error(std::string("nc_inq_varid failed for var ") + var);
    }

    nc_type vtype;
    int ndims = 0;
    int dimids[NC_MAX_VAR_DIMS];
    if (nc_inq_var(ncid, varid, nullptr, &vtype, &ndims, dimids, nullptr) != NC_NOERR) {
        nc_close(ncid);
        throw std::runtime_error("nc_inq_var failed");
    }
    if (ndims != 2) {
        nc_close(ncid);
        throw std::runtime_error("expected 2D var");
    }

    size_t ny = 0, nx = 0;
    if (nc_inq_dimlen(ncid, dimids[0], &ny) != NC_NOERR ||
        nc_inq_dimlen(ncid, dimids[1], &nx) != NC_NOERR) {
        nc_close(ncid);
        throw std::runtime_error("nc_inq_dimlen failed");
    }

    std::vector<double> flat(nx * ny);
    if (vtype == NC_DOUBLE) {
        if (nc_get_var_double(ncid, varid, flat.data()) != NC_NOERR) {
            nc_close(ncid);
            throw std::runtime_error("nc_get_var_double failed");
        }
    } else if (vtype == NC_FLOAT) {
        std::vector<float> f(nx * ny);
        if (nc_get_var_float(ncid, varid, f.data()) != NC_NOERR) {
            nc_close(ncid);
            throw std::runtime_error("nc_get_var_float failed");
        }
        for (size_t i = 0; i < f.size(); ++i) flat[i] = (double)f[i];
    } else {
        nc_close(ncid);
        throw std::runtime_error("unsupported var type");
    }

    nc_close(ncid);

    std::vector<std::vector<double>> grid(ny, std::vector<double>(nx));
    for (size_t j = 0; j < ny; ++j)
        for (size_t i = 0; i < nx; ++i) grid[j][i] = flat[j * nx + i];
    return grid;
}

std::vector<RankTile> read_rank_layout(const fs::path& p) {
    std::ifstream ifs(p);
    if (!ifs)
        throw std::runtime_error("Failed to open " + p.string());

    std::string line;
    if (!std::getline(ifs, line))
        throw std::runtime_error("Empty rank layout: " + p.string());

    std::vector<RankTile> tiles;
    while (std::getline(ifs, line)) {
        if (line.empty())
            continue;
        std::stringstream ss(line);
        std::string tok;
        RankTile t{};
        std::getline(ss, tok, ',');
        t.rank = std::stoi(tok);
        std::getline(ss, tok, ',');
        t.x_off = std::stoi(tok);
        std::getline(ss, tok, ',');
        t.y_off = std::stoi(tok);
        std::getline(ss, tok, ',');
        t.nx = std::stoi(tok);
        std::getline(ss, tok, ',');
        t.ny = std::stoi(tok);
        std::getline(ss, tok, ',');
        t.halo = std::stoi(tok);
        std::getline(ss, tok, ',');
        t.nxg = std::stoi(tok);
        std::getline(ss, tok, ',');
        t.nyg = std::stoi(tok);
        tiles.push_back(t);
    }
    return tiles;
}

std::vector<std::vector<double>> assemble_global_nc_snapshot_from(const std::string& dir,
                                                                  int step,
                                                                  const char* var) {
    auto tiles = read_rank_layout("outputs/rank_layout.csv");
    if (tiles.empty())
        throw std::runtime_error("rank_layout.csv has no tiles");

    const int NX = tiles[0].nxg;
    const int NY = tiles[0].nyg;
    std::vector<std::vector<double>> U(NY, std::vector<double>(NX, 0.0));

    for (const auto& t : tiles) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "snapshot_%05d_rank%05d.nc", step, t.rank);
        fs::path f = fs::path(dir) / buf;
        if (!fs::exists(f))
            throw std::runtime_error("Missing snapshot: " + f.string());

        auto tile = read_nc_2d(f, var);
        if (tile.empty())
            throw std::runtime_error("Empty tile: " + f.string());

        const int ny_nc = (int)tile.size();
        const int nx_nc = (int)tile[0].size();
        int off = 0;
        if (ny_nc == t.ny && nx_nc == t.nx) {
            off = 0;
        } else if (ny_nc == t.ny + 2 * t.halo && nx_nc == t.nx + 2 * t.halo) {
            off = t.halo;
        } else {
            throw std::runtime_error("NetCDF tile shape mismatch");
        }

        for (int j = 0; j < t.ny; ++j)
            for (int i = 0; i < t.nx; ++i) U[t.y_off + j][t.x_off + i] = tile[j + off][i + off];
    }
    return U;
}

double sum2d(const std::vector<std::vector<double>>& A) {
    double s = 0.0;
    for (const auto& r : A)
        for (double v : r) s += v;
    return s;
}

double com_x(const std::vector<std::vector<double>>& A) {
    const int NY = (int)A.size();
    const int NX = (NY > 0 ? (int)A[0].size() : 0);
    double m = 0.0, sx = 0.0;
    for (int j = 0; j < NY; ++j)
        for (int i = 0; i < NX; ++i) {
            m += A[j][i];
            sx += A[j][i] * (i + 0.5);
        }
    return sx / std::max(m, 1e-300);
}
