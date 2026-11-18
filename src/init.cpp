#include "init.hpp"

#include "netcdf_input.hpp"

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

using namespace netcdf_input;

class InitialConditionFile : private ParallelInputFile {
   public:
    InitialConditionFile(MPI_Comm comm, const ICConfig& cfg)
        : ParallelInputFile(comm, cfg.path, "initial condition") {}

    void resolve_grid(SimConfig& cfg) {
        int grid[3] = {cfg.nx, cfg.ny, cfg.nz};
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

            check_netcdf(ncmpi_inq_varndims(ncid_, varid_, &ndims_), "Cannot inquire " + variable);
            if (ndims_ != 2 && ndims_ != 3) {
                throw std::runtime_error(
                    variable + " without a z axis must be 2D (y, x); got rank " +
                    std::to_string(ndims_) + " (3D (z, y, x) also supported).");
            }
            nc_type type;
            int dimids[3];
            check_netcdf(ncmpi_inq_var(ncid_, varid_, nullptr, &type, nullptr, dimids, nullptr),
                         "Cannot inquire " + variable);
            if (type != NC_FLOAT && type != NC_DOUBLE)
                throw std::runtime_error(variable + " must be float or double.");

            MPI_Offset ny, nx;
            check_netcdf(ncmpi_inq_dimlen(ncid_, dimids[ndims_ - 2], &ny),
                         "Cannot inquire y dimension");
            check_netcdf(ncmpi_inq_dimlen(ncid_, dimids[ndims_ - 1], &nx),
                         "Cannot inquire x dimension");
            const auto max_size = std::numeric_limits<int>::max() - 2;  // Space for halos.
            if (nx <= 0 || ny <= 0 || nx > max_size || ny > max_size)
                throw std::runtime_error(variable + " has unsupported grid dimensions.");

            if (ndims_ == 3 || cfg.nz > 1) {
                const char* axes[] = {"z", "y", "x"};
                for (int d = 0; d < ndims_; ++d) {
                    char name[NC_MAX_NAME + 1];
                    check_netcdf(ncmpi_inq_dimname(ncid_, dimids[d], name), variable);
                    if (std::string(name) != axes[3 - ndims_ + d])
                        throw std::runtime_error(variable + " expected dimension '" +
                                                 axes[3 - ndims_ + d] + "', found '" + name + "'");
                }
            }
            if (ndims_ == 3) {
                MPI_Offset nz;
                check_netcdf(ncmpi_inq_dimlen(ncid_, dimids[0], &nz), variable);
                if (nz < 1 || nz > max_size || (cfg.nz_explicit && nz != cfg.nz))
                    throw std::runtime_error(variable +
                                             " z extent does not match the simulation grid");
                grid[2] = static_cast<int>(nz);
            } else if (cfg.nz > 1 && !cfg.ic.replicate_z) {
                throw std::runtime_error(
                    "A 2D initial condition on a 3D grid requires ic.replicate_z: true");
            }

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

        MPI_Bcast(&ndims_, 1, MPI_INT, 0, comm_);
        MPI_Bcast(grid, 3, MPI_INT, 0, comm_);
        MPI_Bcast(&varid_, 1, MPI_INT, 0, comm_);
        cfg.nx = grid[0];
        cfg.ny = grid[1];
        cfg.nz = grid[2];
    }

    void read(const Decomposition& dec, Field& u) const {
        const MPI_Offset start[] = {dec.z_offset, dec.y_offset, dec.x_offset};
        const MPI_Offset count[] = {dec.nz_local, dec.ny_local, dec.nx_local};
        const size_t slice = static_cast<size_t>(dec.ny_local) * dec.nx_local;
        std::vector<double> local(slice * (ndims_ == 3 ? dec.nz_local : 1));

        const int offset = 3 - ndims_;
        const int status =
            ncmpi_get_vara_double_all(ncid_, varid_, start + offset, count + offset, local.data());
        check_netcdf(collective_status(comm_, status), "Cannot read initial condition data");

        const int non_finite = std::any_of(
            local.begin(), local.end(), [](double value) { return !std::isfinite(value); });
        int any_non_finite;
        MPI_Allreduce(&non_finite, &any_non_finite, 1, MPI_INT, MPI_MAX, comm_);
        if (any_non_finite)
            throw std::runtime_error("Initial condition variable contains non-finite values.");

        for (int k = 0; k < dec.nz_local; ++k)
            for (int j = 0; j < dec.ny_local; ++j) {
                if (dec.nx_local)
                    std::copy_n(local.data() + (ndims_ == 3 ? k * slice : 0) +
                                    static_cast<size_t>(j) * dec.nx_local,
                                dec.nx_local,
                                &u.at(u.halo, j + u.halo, k + u.z_halo));
            }
    }

   private:
    int varid_ = -1, ndims_ = 0;
};

void ic_gaussian(const Decomposition& dec, Field& u, const SimConfig& cfg) {
    const int h = u.halo;
    const int nx = u.nx_local;
    const int ny = u.ny_local;

    const double Lx = cfg.nx * cfg.dx;
    const double Ly = cfg.ny * cfg.dy;
    const double xc = cfg.ic.xc_frac * Lx;
    const double yc = cfg.ic.yc_frac * Ly;
    const double Lz = cfg.nz * cfg.dz;
    const double sig = cfg.ic.sigma_frac * (cfg.nz > 1 ? std::min({Lx, Ly, Lz}) : std::min(Lx, Ly));
    for (int k = 0; k < u.nz_local; ++k) {
        const double z =
            cfg.nz > 1 ? ((dec.z_offset + k + 0.5) * cfg.dz - cfg.ic.zc_frac * Lz) : 0.0;

        for (int j = h; j < h + ny; ++j) {
            const int gj = dec.y_offset + (j - h);
            const double y = (gj + 0.5) * cfg.dy;
            for (int i = h; i < h + nx; ++i) {
                const int gi = dec.x_offset + (i - h);
                const double x = (gi + 0.5) * cfg.dx;
                const double r2 = (x - xc) * (x - xc) + (y - yc) * (y - yc);
                u.at(i, j, k + u.z_halo) = cfg.ic.A * std::exp(-(r2 + z * z) / (2.0 * sig * sig));
            }
        }
    }
}

}  // namespace

void initialize_mpi(int& argc, char**& argv) {
#if defined(_OPENMP) || defined(CLIMATE_ENABLE_KOKKOS)
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
#ifdef _OPENMP
    if (provided < MPI_THREAD_FUNNELED)
        omp_set_num_threads(1);
#endif
#else
    MPI_Init(&argc, &argv);
#endif
}

void apply_initial_condition(const Decomposition& dec, Field& u, const SimConfig& cfg) {
    if (cfg.ic.mode == "preset") {
        if (cfg.ic.preset == "gaussian_hotspot") {
            ic_gaussian(dec, u, cfg);
        } else if (cfg.ic.preset == "constant_zero") {
            /* no-op, u already zero */
        } else {
            throw std::runtime_error("Unknown IC preset: " + cfg.ic.preset);
        }
    } else if (cfg.ic.mode == "file") {
        InitialConditionFile file(dec.io_comm == MPI_COMM_NULL ? dec.cart_comm : dec.io_comm,
                                  cfg.ic);
        SimConfig grid = cfg;
        grid.nx = dec.nx_global;
        grid.ny = dec.ny_global;
        grid.nz = dec.nz_global;
        grid.nx_explicit = grid.ny_explicit = grid.nz_explicit = true;
        file.resolve_grid(grid);
        file.read(dec, u);
    } else {
        throw std::runtime_error("Unknown IC mode: " + cfg.ic.mode);
    }
}

Field initialize_field(MPI_Comm comm, Decomposition& dec, SimConfig& cfg) {
    std::optional<InitialConditionFile> file;
    if (cfg.ic.mode == "file") {
        file.emplace(comm, cfg.ic);
        file->resolve_grid(cfg);
        cfg.validate();
    }

    dec.init(comm,
             cfg.nx,
             cfg.ny,
             cfg.nz,
             cfg.bc.left == BCType::Periodic,
             cfg.bc.bottom == BCType::Periodic,
             cfg.bc.front == BCType::Periodic);
    if (file && (cfg.nx < dec.dims[0] || cfg.ny < dec.dims[1]))
        throw std::runtime_error("Initial condition grid is too small for the MPI decomposition.");

    Field u(dec, 1, cfg.dx, cfg.dy, cfg.dz);
    if (file)
        file->read(dec, u);
    else
        apply_initial_condition(dec, u, cfg);
    return u;
}
