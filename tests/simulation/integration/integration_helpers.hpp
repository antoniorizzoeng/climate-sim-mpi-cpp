#pragma once
#include <cstdlib>
#include <filesystem>
namespace fs = std::filesystem;
#include <netcdf.h>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

struct RankTile {
    int rank;
    int x_off;
    int y_off;
    int nx;
    int ny;
    int halo;
    int nxg;
    int nyg;
};

int run_cmd(const std::string& cmd);

std::vector<std::vector<double>> read_nc_2d(const fs::path& p, const char* var);

std::vector<RankTile> read_rank_layout(const fs::path& csv);

std::vector<std::vector<double>> assemble_global_nc_snapshot_from(const std::string& dir,
                                                                  int step,
                                                                  const char* var);

double sum2d(const std::vector<std::vector<double>>& A);
double com_x(const std::vector<std::vector<double>>& A);
