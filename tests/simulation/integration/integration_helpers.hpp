#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs = std::filesystem;

#ifndef MPIEXEC_EXECUTABLE
#define MPIEXEC_EXECUTABLE "mpirun"
#endif
#ifndef MPIEXEC_NUMPROC_FLAG
#define MPIEXEC_NUMPROC_FLAG "-np"
#endif
#ifndef MPIEXEC_PREFLAGS
#define MPIEXEC_PREFLAGS ""
#endif
#ifndef INTEGRATION_MPI_PROCS
#define INTEGRATION_MPI_PROCS 4
#endif

int run_mpi_cmd(const std::string& exe, const std::vector<std::string>& args);
std::vector<std::vector<double>> read_nc_2d(const fs::path& file, int step, const char* var);
double sum2d(const std::vector<std::vector<double>>& A);
double com_x(const std::vector<std::vector<double>>& A);
