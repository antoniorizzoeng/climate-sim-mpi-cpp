#include "wind.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>

#include "stability.hpp"

using netcdf_input::check_netcdf;

WindFile::WindFile(const Decomposition& dec, const SimConfig& cfg)
    : ParallelInputFile(dec.io_comm, cfg.wind.path, "wind"),
      dec_(dec),
      interval_(cfg.wind.interval) {
    const bool volume = dec.nz_global > 1;
    const int components = volume && !cfg.wind.vz_var.empty() ? 3 : 2;
    const int spatial = volume ? 3 : 2;
    netcdf_input::inspect_on_root(comm_, [&] {
        const std::string names[] = {cfg.wind.vx_var, cfg.wind.vy_var, cfg.wind.vz_var};
        const char* axes[] = {"time", "z", "y", "x"};
        const int extents[] = {0, dec.nz_global, dec.ny_global, dec.nx_global};
        for (int component = 0; component < components; ++component) {
            const std::string var = "Wind variable '" + names[component] + "'";
            int id, rank;
            check_netcdf(ncmpi_inq_varid(ncid_, names[component].c_str(), &id), var);
            varids_[component] = id;
            check_netcdf(ncmpi_inq_varndims(ncid_, id, &rank), var);
            if (rank != spatial && rank != spatial + 1)
                throw std::runtime_error(
                    var + " must have dimensions " +
                    (volume ? "(time, z, y, x) or (z, y, x)" : "(time, y, x) or (y, x)"));
            if (component != 0 && rank != ndims_)
                throw std::runtime_error("Wind components must have matching dimensions");
            ndims_ = rank;
            int ids[4];
            nc_type type;
            check_netcdf(ncmpi_inq_var(ncid_, id, nullptr, &type, nullptr, ids, nullptr), var);
            if (type == NC_CHAR)
                throw std::runtime_error(var + " must contain numeric velocities");
            for (int d = 0; d < rank; ++d) {
                char name[NC_MAX_NAME + 1];
                MPI_Offset length;
                check_netcdf(ncmpi_inq_dim(ncid_, ids[d], name, &length), var);
                const int axis = (d == 0 && rank > spatial) ? 0 : d + 4 - rank;
                if (std::string(name) != axes[axis])
                    throw std::runtime_error(var + " expected dimension '" + axes[axis] +
                                             "', found '" + name + "'");
                if (axis != 0 && length != extents[axis])
                    throw std::runtime_error(var + " dimensions do not match the simulation grid");
                if (length == 0)
                    throw std::runtime_error(var + " has an empty dimension");
                if (axis == 0)
                    records_ = length;
            }
        }
    });
    MPI_Bcast(varids_, 3, MPI_INT, 0, comm_);
    MPI_Bcast(&ndims_, 1, MPI_INT, 0, comm_);
    MPI_Bcast(&records_, 1, MPI_OFFSET, 0, comm_);

    // Scan backwards so records 0 and 1 are already cached for the first step.
    std::array<double, 3> maxima{};
    for (MPI_Offset end = records_; end > 0; --end) {
        auto& record = end == 1 ? lower_ : upper_;
        read(end - 1, record);
        auto update = [](double& maximum, const std::vector<double>& values) {
            for (double v : values)
                maximum = std::isfinite(v) ? std::max(maximum, std::abs(v))
                                           : std::numeric_limits<double>::infinity();
        };
        update(maxima[0], record.tile.vx);
        update(maxima[1], record.tile.vy);
        update(maxima[2], record.tile.vz);
    }
    MPI_Allreduce(MPI_IN_PLACE, maxima.data(), 3, MPI_DOUBLE, MPI_MAX, comm_);
    if (!std::isfinite(maxima[0]) || !std::isfinite(maxima[1]) || !std::isfinite(maxima[2]))
        throw std::runtime_error("Wind file contains non-finite velocities");
    const double limit = combined_dt(cfg.dx,
                                     cfg.dy,
                                     maxima[0],
                                     maxima[1],
                                     cfg.D,
                                     cfg.dz,
                                     maxima[2],
                                     volume ? cfg.vertical_diffusion() : 0);
    if (cfg.dt > limit) {
        std::ostringstream error;
        error.precision(std::numeric_limits<double>::max_digits10);
        error << "dt=" << cfg.dt << " exceeds file wind combined stability limit " << limit;
        throw std::runtime_error(error.str());
    }
}

void WindFile::read(MPI_Offset index, Record& record) {
    const bool volume = dec_.nz_global > 1;
    const MPI_Offset start[] = {
        index, volume ? dec_.z_offset : index, dec_.y_offset, dec_.x_offset};
    const MPI_Offset count[] = {1, volume ? dec_.nz_local : 1, dec_.ny_local, dec_.nx_local};
    const int offset = 4 - ndims_;
    auto read_component = [&](int id, std::vector<double>& values) {
        values.resize(static_cast<size_t>(dec_.ny_local) * dec_.nx_local * dec_.nz_local);
        int status =
            ncmpi_get_vara_double_all(ncid_, id, start + offset, count + offset, values.data());
        check_netcdf(netcdf_input::collective_status(comm_, status), "Cannot read wind tile");
    };
    read_component(varids_[0], record.tile.vx);
    read_component(varids_[1], record.tile.vy);
    if (varids_[2] >= 0)
        read_component(varids_[2], record.tile.vz);
    record.index = index;
}

WindSample WindFile::sample(double time) {
    const double position = time / interval_;
    const MPI_Offset last = records_ - 1;
    const MPI_Offset index =
        position >= static_cast<double>(last) ? last : static_cast<MPI_Offset>(position);
    if (index == upper_.index)
        std::swap(lower_, upper_);
    if (index != lower_.index)
        read(index, lower_);
    if (index == last || position == static_cast<double>(index))
        return {lower_.tile, lower_.tile, 0, index, index};
    if (index + 1 != upper_.index)
        read(index + 1, upper_);
    return {lower_.tile, upper_.tile, position - static_cast<double>(index), index, index + 1};
}
