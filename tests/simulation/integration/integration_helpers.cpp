#include "integration_helpers.hpp"

#include <netcdf.h>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

inline void nc_check(int status, const char* where) {
    if (status != NC_NOERR) {
        throw std::runtime_error(std::string(where) + ": " + nc_strerror(status));
    }
}

int run_mpi_cmd(const std::string& exe, const std::vector<std::string>& args) {
    std::ostringstream oss;
    oss << MPIEXEC_EXECUTABLE << " " << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG << " "
        << INTEGRATION_MPI_PROCS << " " << exe;
    for (const auto& a : args) oss << " " << a;
    std::string cmd = oss.str();
    std::cout << "[MPI CMD] " << cmd << "\n";
    return std::system(cmd.c_str());
}

std::vector<std::vector<double>> read_nc_2d(const fs::path& file, int step, const char* var) {
    int ncid, varid;
    nc_check(nc_open(file.string().c_str(), NC_NOWRITE, &ncid), "nc_open");
    nc_check(nc_inq_varid(ncid, var, &varid), "nc_inq_varid");

    int ndims;
    nc_check(nc_inq_varndims(ncid, varid, &ndims), "nc_inq_varndims");
    if (ndims != 2 && ndims != 3) {
        nc_close(ncid);
        throw std::runtime_error("unsupported rank " + std::to_string(ndims));
    }

    int dimids[NC_MAX_VAR_DIMS];
    nc_check(nc_inq_vardimid(ncid, varid, dimids), "nc_inq_vardimid");

    size_t sizes[3] = {1, 1, 1};
    for (int d = 0; d < ndims; ++d)
        nc_check(nc_inq_dimlen(ncid, dimids[d], &sizes[d]), "nc_inq_dimlen");

    size_t NX = 0, NY = 0;
    std::vector<double> flat;
    if (ndims == 3) {
        if (step < 0 || step >= (int)sizes[0]) {
            nc_close(ncid);
            throw std::runtime_error("step out of range");
        }
        NX = sizes[2];
        NY = sizes[1];
        flat.resize(NX * NY);
        size_t start[3] = {(size_t)step, 0, 0}, count[3] = {1, NY, NX};
        nc_check(nc_get_vara_double(ncid, varid, start, count, flat.data()),
                 "nc_get_vara_double 3D");
    } else {
        NX = sizes[1];
        NY = sizes[0];
        flat.resize(NX * NY);
        size_t start[2] = {0, 0}, count[2] = {NY, NX};
        nc_check(nc_get_vara_double(ncid, varid, start, count, flat.data()),
                 "nc_get_vara_double 2D");
    }

    nc_check(nc_close(ncid), "nc_close");

    std::vector<std::vector<double>> grid(NY, std::vector<double>(NX));
    for (size_t j = 0; j < NY; ++j)
        for (size_t i = 0; i < NX; ++i) grid[j][i] = flat[j * NX + i];
    return grid;
}

double sum2d(const std::vector<std::vector<double>>& A) {
    double s = 0.0;
    for (auto& r : A)
        for (double v : r) s += v;
    return s;
}
double com_x(const std::vector<std::vector<double>>& A) {
    const int NY = (int)A.size();
    const int NX = NY ? (int)A[0].size() : 0;
    double m = 0.0, sx = 0.0;
    for (int j = 0; j < NY; ++j)
        for (int i = 0; i < NX; ++i) {
            double v = A[j][i];
            m += v;
            sx += v * (i + 0.5);
        }
    return sx / std::max(m, 1e-300);
}
