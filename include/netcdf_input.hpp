#pragma once

#include <pnetcdf.h>

#include <stdexcept>
#include <string>

namespace netcdf_input {
inline void check_netcdf(int status, const std::string& action) {
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

inline int collective_status(MPI_Comm comm, int status) {
    int result;
    MPI_Allreduce(&status, &result, 1, MPI_INT, MPI_MIN, comm);
    return result;
}

class ParallelInputFile {
   protected:
    ParallelInputFile(MPI_Comm comm, const std::string& path, const std::string& source)
        : comm_(comm) {
        if (comm == MPI_COMM_NULL)
            throw std::runtime_error("File input requires an MPI communicator.");

        const std::string open_error = "Cannot open " + source + " file '" + path + "'";
        inspect_on_root(comm_, [&] {
            int format;
            const int status = ncmpi_inq_file_format(path.c_str(), &format);
            check_netcdf(status, open_error);
            if (format == NC_FORMAT_NETCDF4 || format == NC_FORMAT_NETCDF4_CLASSIC) {
                throw std::runtime_error(open_error +
                                         ": PnetCDF cannot read NetCDF-4/HDF5; convert to CDF-5 "
                                         "(nccopy -k cdf5 input.nc output.nc).");
            }
            if (format != NC_FORMAT_CLASSIC && format != NC_FORMAT_CDF2 && format != NC_FORMAT_CDF5)
                throw std::runtime_error(open_error + ": expected a CDF-1, CDF-2, or CDF-5 file.");
        });

        int status = ncmpi_open(comm_, path.c_str(), NC_NOWRITE, MPI_INFO_NULL, &ncid_);
        check_netcdf(collective_status(comm_, status), open_error);
    }

    ParallelInputFile(const ParallelInputFile&) = delete;
    ParallelInputFile& operator=(const ParallelInputFile&) = delete;

    ~ParallelInputFile() { ncmpi_close(ncid_); }

    MPI_Comm comm_;
    int ncid_ = -1;
};
}  // namespace netcdf_input
