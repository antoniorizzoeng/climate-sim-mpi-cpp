#include "init.hpp"

#include <pnetcdf.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check_netcdf(int status, const std::string& action) {
    if (status != NC_NOERR)
        throw std::runtime_error(action + ": " + ncmpi_strerror(status));
}

// Only rank 0 inspects the header. Propagate its result before any further collective.
template <typename Inspect>
void inspect_on_root(MPI_Comm comm, Inspect inspect) {
    int rank;
    MPI_Comm_rank(comm, &rank);
    std::string error;
    if (rank == 0) {
        try {
            inspect();
        } catch (const std::exception& e) {
            error = e.what();
        }
    }
    int length = static_cast<int>(error.size());
    MPI_Bcast(&length, 1, MPI_INT, 0, comm);
    if (length != 0) {
        error.resize(length);
        MPI_Bcast(error.data(), length, MPI_CHAR, 0, comm);
        throw std::runtime_error(error);
    }
}

int collective_status(MPI_Comm comm, int status) {
    int result;
    MPI_Allreduce(&status, &result, 1, MPI_INT, MPI_MIN, comm);
    return result;
}

class InitialConditionFile {
   public:
    InitialConditionFile(MPI_Comm comm, const ICConfig& cfg) : comm_(comm) {
        if (comm == MPI_COMM_NULL)
            throw std::runtime_error("File initial conditions require an MPI communicator.");

        const std::string open_error = "Cannot open initial condition file '" + cfg.path + "'";
        inspect_on_root(comm_, [&] {
            int format;
            const int status = ncmpi_inq_file_format(cfg.path.c_str(), &format);
            check_netcdf(status, open_error);
            if (format == NC_FORMAT_NETCDF4 || format == NC_FORMAT_NETCDF4_CLASSIC) {
                throw std::runtime_error(open_error +
                                         ": PnetCDF cannot read NetCDF-4/HDF5; convert to CDF-5 "
                                         "(nccopy -k cdf5 input.nc output.nc).");
            }
            if (format != NC_FORMAT_CLASSIC && format != NC_FORMAT_CDF2 && format != NC_FORMAT_CDF5)
                throw std::runtime_error(open_error + ": expected a CDF-1, CDF-2, or CDF-5 file.");
        });

        int status = ncmpi_open(comm_, cfg.path.c_str(), NC_NOWRITE, MPI_INFO_NULL, &ncid_);
        check_netcdf(collective_status(comm_, status), open_error);
    }

    InitialConditionFile(const InitialConditionFile&) = delete;
    InitialConditionFile& operator=(const InitialConditionFile&) = delete;

    ~InitialConditionFile() { ncmpi_close(ncid_); }

    void resolve_grid(SimConfig& cfg) {
        int grid[2] = {cfg.nx, cfg.ny};
        inspect_on_root(comm_, [&] {
            const std::string variable = "Initial condition variable '" + cfg.ic.var + "'";
            int status = ncmpi_inq_varid(ncid_, cfg.ic.var.c_str(), &varid_);
            if (status == NC_ENOTVAR) {
                int nvars;
                check_netcdf(ncmpi_inq_nvars(ncid_, &nvars),
                             "Cannot list initial condition variables");
                std::ostringstream error;
                error << variable << " not found in '" << cfg.ic.path << "'. Available: ";
                if (nvars == 0)
                    error << "(none)";
                for (int i = 0; i < nvars; ++i) {
                    char name[NC_MAX_NAME + 1];
                    check_netcdf(ncmpi_inq_varname(ncid_, i, name),
                                 "Cannot inquire initial condition variable name");
                    if (i != 0)
                        error << ", ";
                    error << name;
                }
                throw std::runtime_error(error.str());
            }
            check_netcdf(status, "Cannot look up " + variable);

            int ndims;
            check_netcdf(ncmpi_inq_varndims(ncid_, varid_, &ndims), "Cannot inquire " + variable);
            if (ndims != 2) {
                throw std::runtime_error(variable + " must be 2D (y, x); got rank " +
                                         std::to_string(ndims) + ".");
            }
            nc_type type;
            int dimids[2];
            check_netcdf(ncmpi_inq_var(ncid_, varid_, nullptr, &type, nullptr, dimids, nullptr),
                         "Cannot inquire " + variable);
            if (type != NC_FLOAT && type != NC_DOUBLE)
                throw std::runtime_error(variable + " must be float or double.");

            MPI_Offset ny, nx;
            check_netcdf(ncmpi_inq_dimlen(ncid_, dimids[0], &ny), "Cannot inquire y dimension");
            check_netcdf(ncmpi_inq_dimlen(ncid_, dimids[1], &nx), "Cannot inquire x dimension");
            const auto max_size = std::numeric_limits<int>::max() - 2;  // Space for halos.
            if (nx <= 0 || ny <= 0 || nx > max_size || ny > max_size)
                throw std::runtime_error(variable + " has unsupported grid dimensions.");

            if (!cfg.nx_explicit)
                grid[0] = static_cast<int>(nx);
            if (!cfg.ny_explicit)
                grid[1] = static_cast<int>(ny);
            if (nx != grid[0] || ny != grid[1]) {
                std::ostringstream error;
                error << variable << " size (" << ny << ", " << nx
                      << ") does not match configured grid (ny, nx) = (" << grid[1] << ", "
                      << grid[0] << ").";
                throw std::runtime_error(error.str());
            }
        });

        MPI_Bcast(grid, 2, MPI_INT, 0, comm_);
        MPI_Bcast(&varid_, 1, MPI_INT, 0, comm_);
        cfg.nx = grid[0];
        cfg.ny = grid[1];
    }

    void read(const Decomp2D& dec, Field& u) const {
        const MPI_Offset start[2] = {dec.y_offset, dec.x_offset};
        const MPI_Offset count[2] = {dec.ny_local, dec.nx_local};
        std::vector<double> local(static_cast<size_t>(dec.nx_local) * dec.ny_local);

        // PnetCDF converts NC_FLOAT to double when needed.
        const int status = ncmpi_get_vara_double_all(ncid_, varid_, start, count, local.data());
        check_netcdf(collective_status(comm_, status), "Cannot read initial condition data");

        const int non_finite = std::any_of(
            local.begin(), local.end(), [](double value) { return !std::isfinite(value); });
        int any_non_finite;
        MPI_Allreduce(&non_finite, &any_non_finite, 1, MPI_INT, MPI_MAX, comm_);
        if (any_non_finite)
            throw std::runtime_error("Initial condition variable contains non-finite values.");

        for (int j = 0; j < dec.ny_local; ++j) {
            for (int i = 0; i < dec.nx_local; ++i)
                u.at(i + u.halo, j + u.halo) = local[static_cast<size_t>(j) * dec.nx_local + i];
        }
    }

   private:
    MPI_Comm comm_;
    int ncid_ = -1;
    int varid_ = -1;
};

void ic_gaussian(const Decomp2D& dec, Field& u, const SimConfig& cfg) {
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

}  // namespace

void initialize_mpi(int& argc, char**& argv) {
#ifdef _OPENMP
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    if (provided < MPI_THREAD_FUNNELED)
        omp_set_num_threads(1);
#else
    MPI_Init(&argc, &argv);
#endif
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
        InitialConditionFile file(dec.cart_comm, cfg.ic);
        SimConfig grid = cfg;
        grid.nx = dec.nx_global;
        grid.ny = dec.ny_global;
        grid.nx_explicit = grid.ny_explicit = true;
        file.resolve_grid(grid);
        file.read(dec, u);
    } else {
        throw std::runtime_error("Unknown IC mode: " + cfg.ic.mode);
    }
}

Field initialize_field(MPI_Comm comm, Decomp2D& dec, SimConfig& cfg) {
    std::optional<InitialConditionFile> file;
    if (cfg.ic.mode == "file") {
        file.emplace(comm, cfg.ic);
        file->resolve_grid(cfg);
    }

    dec.init(comm, cfg.nx, cfg.ny);
    if (file && (cfg.nx < dec.dims[0] || cfg.ny < dec.dims[1]))
        throw std::runtime_error("Initial condition grid is too small for the MPI decomposition.");

    Field u(dec.nx_local, dec.ny_local, 1, cfg.dx, cfg.dy);
    if (file)
        file->read(dec, u);
    else
        apply_initial_condition(dec, u, cfg);
    return u;
}
