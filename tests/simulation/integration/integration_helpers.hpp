#pragma once
#include <cstdlib>
#include <filesystem>
namespace fs = std::filesystem;
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef HAS_NETCDF
#include <netcdf.h>
#endif

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

std::vector<std::vector<double>> read_csv_2d(const fs::path& p);
std::vector<double> read_bin_plane(const fs::path& p, int nx, int ny);

#ifdef HAS_NETCDF
std::vector<std::vector<double>> read_nc_2d(const fs::path& p, const char* var);
#endif

std::vector<RankTile> read_rank_layout(const fs::path& csv);

std::vector<std::vector<double>> assemble_global_csv_snapshot(int step);
std::vector<std::vector<double>> assemble_global_csv_snapshot_from(const std::string& dir,
                                                                   int step);

#ifdef HAS_NETCDF
std::vector<std::vector<double>> assemble_global_nc_snapshot_from(const std::string& dir,
                                                                  int step,
                                                                  const char* var);
#endif

double sum2d(const std::vector<std::vector<double>>& A);
double com_x(const std::vector<std::vector<double>>& A);
