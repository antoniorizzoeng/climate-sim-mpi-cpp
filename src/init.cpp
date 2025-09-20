#include "init.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <netcdf>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static void ic_gaussian(const Decomp2D& dec, Field& u, const SimConfig& cfg) {
    const int h = u.halo;
    const int nx = u.nx_local;
    const int ny = u.ny_local;

    const double Lx = cfg.nx * cfg.dx;
    const double Ly = cfg.ny * cfg.dy;
    const double xc = cfg.ic.xc_frac * Lx;
    const double yc = cfg.ic.yc_frac * Ly;
    const double sig = cfg.ic.sigma_frac * std::min(Lx, Ly);

    for (int j = h; j < h + ny; ++j) {
        const int gj = dec.y_offset + (j - h);
        const double y = (gj + 0.5) * cfg.dy;
        for (int i = h; i < h + nx; ++i) {
            const int gi = dec.x_offset + (i - h);
            const double x = (gi + 0.5) * cfg.dx;
            const double r2 = (x - xc) * (x - xc) + (y - yc) * (y - yc);
            u.at(i, j) = cfg.ic.A * std::exp(-r2 / (2.0 * sig * sig));
        }
    }
}

static void ic_from_bin_global(const Decomp2D& dec, Field& u, const SimConfig& cfg) {
    const int NX = cfg.nx, NY = cfg.ny;
    std::ifstream ifs(cfg.ic.path, std::ios::binary);
    if (!ifs)
        throw std::runtime_error("IC file open failed: " + cfg.ic.path);

    const int h = u.halo;
    const int nx = u.nx_local;
    const int ny = u.ny_local;

    for (int j = 0; j < ny; ++j) {
        const int gj = dec.y_offset + j;
        const std::streamoff row_off = static_cast<std::streamoff>(gj) * NX * sizeof(double);
        const std::streamoff col_off = static_cast<std::streamoff>(dec.x_offset) * sizeof(double);
        ifs.seekg(row_off + col_off, std::ios::beg);
        for (int i = 0; i < nx; ++i) {
            double val{};
            ifs.read(reinterpret_cast<char*>(&val), sizeof(double));
            u.at(h + i, h + j) = val;
        }
        if (!ifs)
            throw std::runtime_error("IC file read failed at row " + std::to_string(gj));
    }
}

void apply_initial_condition(const Decomp2D& dec, Field& u, const SimConfig& cfg) {
    if (cfg.ic.mode == "preset") {
        if (cfg.ic.preset == "gaussian_hotspot") {
            ic_gaussian(dec, u, cfg);
        } else if (cfg.ic.preset == "constant_zero") {
            /* no-op, u already zero */
        } else {
            throw std::runtime_error("Unknown IC preset: " + cfg.ic.preset);
        }
    } else if (cfg.ic.mode == "file") {
        if (cfg.ic.format == "bin") {
            ic_from_bin_global(dec, u, cfg);
        } else if (cfg.ic.format == "netcdf" || cfg.ic.format == "nc") {
            const std::string varname = cfg.ic.var.empty() ? "T" : cfg.ic.var;
            if (!ic_from_netcdf_global(dec, u, cfg.ic.path, varname)) {
                throw std::runtime_error("Failed to read NetCDF variable '" + varname +
                                         "' from file: " + cfg.ic.path);
            }
        } else {
            throw std::runtime_error("Unsupported IC format: " + cfg.ic.format);
        }
    }
}

static inline void nc_check(int status, const char* where) {
    if (status != NC_NOERR) {
        std::ostringstream oss;
        oss << where << ": " << nc_strerror(status);
        throw std::runtime_error(oss.str());
    }
}

bool ic_from_netcdf_global(const Decomp2D& dec,
                           Field& u,
                           const std::string& path,
                           const std::string& varname) {
    int ncid;
    int status = nc_open(path.c_str(), NC_NOWRITE, &ncid);
    if (status != NC_NOERR) {
        throw std::runtime_error("Failed to open NetCDF file: " + path);
    }

    int varid = -1;
    status = nc_inq_varid(ncid, varname.c_str(), &varid);
    if (status != NC_NOERR) {
        int nvars = 0;
        nc_check(nc_inq_nvars(ncid, &nvars), "nc_inq_nvars");
        std::vector<std::string> names;
        names.reserve(nvars);
        for (int vid = 0; vid < nvars; ++vid) {
            char nm[NC_MAX_NAME + 1];
            nc_check(nc_inq_varname(ncid, vid, nm), "nc_inq_varname");
            names.emplace_back(nm);
        }
        nc_close(ncid);
        std::ostringstream oss;
        oss << "Variable '" << varname << "' not found in " << path << ". Available: ";
        for (size_t i = 0; i < names.size(); ++i) {
            if (i)
                oss << ", ";
            oss << names[i];
        }
        throw std::runtime_error(oss.str());
    }

    nc_type vtype;
    int ndims = 0;
    int dimids[NC_MAX_VAR_DIMS];
    nc_check(nc_inq_var(ncid, varid, nullptr, &vtype, &ndims, dimids, nullptr), "nc_inq_var");

    if (ndims != 2) {
        nc_close(ncid);
        throw std::runtime_error("Expected 2D variable '" + varname + "' in " + path);
    }

    size_t dimlen[2];
    for (int d = 0; d < 2; ++d) {
        nc_check(nc_inq_dimlen(ncid, dimids[d], &dimlen[d]), "nc_inq_dimlen");
    }
    size_t ny = dimlen[0], nx = dimlen[1];
    if ((int)nx != dec.nx_global || (int)ny != dec.ny_global) {
        nc_close(ncid);
        std::ostringstream oss;
        oss << "NetCDF var '" << varname << "' size is (" << ny << "," << nx
            << ") but cfg grid is (" << dec.ny_global << "," << dec.nx_global << ")";
        throw std::runtime_error(oss.str());
    }

    std::vector<double> plane;
    plane.resize(nx * ny);

    if (vtype == NC_DOUBLE) {
        nc_check(nc_get_var_double(ncid, varid, plane.data()), "nc_get_var_double");
    } else if (vtype == NC_FLOAT) {
        std::vector<float> buf(nx * ny);
        nc_check(nc_get_var_float(ncid, varid, buf.data()), "nc_get_var_float");
        for (size_t k = 0; k < buf.size(); ++k) plane[k] = static_cast<double>(buf[k]);
    } else {
        nc_close(ncid);
        throw std::runtime_error("Variable '" + varname + "' must be float or double.");
    }

    nc_close(ncid);

    const int h = u.halo;
    for (int j = 0; j < dec.ny_local; ++j) {
        int gj = dec.y_offset + j;
        const double* src_row = &plane[(size_t)gj * nx];
        for (int i = 0; i < dec.nx_local; ++i) {
            int gi = dec.x_offset + i;
            u.at(i + h, j + h) = src_row[gi];
        }
    }
    return true;
}
