#include "wind.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>

using netcdf_input::check_netcdf;

WindFile::WindFile(const Decomp2D& dec, const SimConfig& cfg)
    : ParallelInputFile(dec.cart_comm, cfg.wind.path, "wind"),
      dec_(dec),
      interval_(cfg.wind.interval) {
    netcdf_input::inspect_on_root(comm_, [&] {
        const std::string names[] = {cfg.wind.vx_var, cfg.wind.vy_var};
        const char* axes[] = {"time", "y", "x"};
        for (int component = 0; component < 2; ++component) {
            const std::string var = "Wind variable '" + names[component] + "'";
            int id, rank;
            check_netcdf(ncmpi_inq_varid(ncid_, names[component].c_str(), &id), var);
            varids_[component] = id;
            check_netcdf(ncmpi_inq_varndims(ncid_, id, &rank), var);
            if (rank != 2 && rank != 3)
                throw std::runtime_error(var + " must have dimensions (time, y, x) or (y, x)");
            if (component == 1 && rank != ndims_)
                throw std::runtime_error("Wind components must have matching dimensions");
            ndims_ = rank;
            int ids[3];
            nc_type type;
            check_netcdf(ncmpi_inq_var(ncid_, id, nullptr, &type, nullptr, ids, nullptr), var);
            if (type == NC_CHAR)
                throw std::runtime_error(var + " must contain numeric velocities");
            for (int d = 0; d < rank; ++d) {
                char name[NC_MAX_NAME + 1];
                MPI_Offset length;
                check_netcdf(ncmpi_inq_dim(ncid_, ids[d], name, &length), var);
                const int axis = d + 3 - rank;
                if (std::string(name) != axes[axis])
                    throw std::runtime_error(var + " expected dimension '" + axes[axis] +
                                             "', found '" + name + "'");
                if ((axis == 1 && length != dec.ny_global) ||
                    (axis == 2 && length != dec.nx_global))
                    throw std::runtime_error(var + " dimensions do not match the simulation grid");
                if (length == 0)
                    throw std::runtime_error(var + " has an empty dimension");
                if (axis == 0)
                    records_ = length;
            }
        }
    });
    MPI_Bcast(varids_, 2, MPI_INT, 0, comm_);
    MPI_Bcast(&ndims_, 1, MPI_INT, 0, comm_);
    MPI_Bcast(&records_, 1, MPI_OFFSET, 0, comm_);

    // Scan backwards so records 0 and 1 are already cached for the first step.
    std::array<double, 2> maxima{};
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
    }
    MPI_Allreduce(MPI_IN_PLACE, maxima.data(), 2, MPI_DOUBLE, MPI_MAX, comm_);
    if (!std::isfinite(maxima[0]) || !std::isfinite(maxima[1]))
        throw std::runtime_error("Wind file contains non-finite velocities");
    const long double dx = cfg.dx, dy = cfg.dy;
    const long double rate =
        maxima[0] / dx + maxima[1] / dy + 2 * ((cfg.D / dx) / dx + (cfg.D / dy) / dy);
    const double limit =
        rate == 0 ? std::numeric_limits<double>::infinity() : static_cast<double>(1 / rate);
    if (cfg.dt > limit) {
        std::ostringstream error;
        error.precision(std::numeric_limits<double>::max_digits10);
        error << "dt=" << cfg.dt << " exceeds file wind combined stability limit " << limit;
        throw std::runtime_error(error.str());
    }
}

void WindFile::read(MPI_Offset index, Record& record) {
    const MPI_Offset start[] = {index, dec_.y_offset, dec_.x_offset};
    const MPI_Offset count[] = {1, dec_.ny_local, dec_.nx_local};
    const int offset = 3 - ndims_;
    auto read_component = [&](int id, std::vector<double>& values) {
        values.resize(static_cast<size_t>(dec_.ny_local) * dec_.nx_local);
        int status =
            ncmpi_get_vara_double_all(ncid_, id, start + offset, count + offset, values.data());
        check_netcdf(netcdf_input::collective_status(comm_, status), "Cannot read wind tile");
    };
    read_component(varids_[0], record.tile.vx);
    read_component(varids_[1], record.tile.vy);
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
        return {lower_.tile, lower_.tile, 0};
    if (index + 1 != upper_.index)
        read(index + 1, upper_);
    return {lower_.tile, upper_.tile, position - static_cast<double>(index)};
}
