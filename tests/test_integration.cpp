#include <gtest/gtest.h>

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

#ifndef CLIMATE_SIM_EXE
#error "CLIMATE_SIM_EXE must be defined by CMake to the built climate_sim path"
#endif
#ifndef PYTHON_EXECUTABLE
#error "PYTHON_EXECUTABLE must be defined by CMake"
#endif
#ifndef SCRIPTS_DIR
#error "SCRIPTS_DIR must be defined by CMake"
#endif
#ifndef MPIEXEC_EXECUTABLE
#define MPIEXEC_EXECUTABLE "mpirun"
#endif
#ifndef MPIEXEC_NUMPROC_FLAG
#define MPIEXEC_NUMPROC_FLAG "-np"
#endif

namespace fs = std::filesystem;

// ---------- Helpers ----------
struct RankTile {
    int rank, x_off, y_off, nx, ny, halo, nxg, nyg;
};

static std::vector<std::vector<double>> read_csv_2d(const fs::path& p) {
    std::ifstream ifs(p);
    if (!ifs.good()) {
        throw std::runtime_error("Failed to open " + p.string());
    }
    std::vector<std::vector<double>> grid;
    std::string line;
    while (std::getline(ifs, line)) {
        std::vector<double> row;
        std::stringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, ',')) {
            if (!tok.empty())
                row.push_back(std::stod(tok));
        }
        if (!row.empty())
            grid.push_back(std::move(row));
    }
    return grid;
}

static std::vector<double> read_bin_plane(const fs::path& p, int nx, int ny) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs.good()) {
        throw std::runtime_error("Failed to open " + p.string());
    }
    std::vector<double> buf((size_t)nx * ny);
    ifs.read(reinterpret_cast<char*>(buf.data()), (std::streamsize)buf.size() * sizeof(double));
    if (!ifs) {
        throw std::runtime_error("Short read on " + p.string());
    }
    return buf;
}

#ifdef HAS_NETCDF
static std::vector<std::vector<double>> read_nc_2d(const fs::path& p, const char* var) {
    int ncid;
    if (nc_open(p.c_str(), NC_NOWRITE, &ncid) != NC_NOERR)
        throw std::runtime_error("nc_open failed for " + p.string());

    int varid;
    if (nc_inq_varid(ncid, var, &varid) != NC_NOERR) {
        nc_close(ncid);
        throw std::runtime_error(std::string("nc_inq_varid failed for var ") + var);
    }

    nc_type vtype;
    int ndims = 0;
    int dimids[NC_MAX_VAR_DIMS];
    if (nc_inq_var(ncid, varid, nullptr, &vtype, &ndims, dimids, nullptr) != NC_NOERR) {
        nc_close(ncid);
        throw std::runtime_error("nc_inq_var failed");
    }
    if (ndims != 2) {
        nc_close(ncid);
        throw std::runtime_error("expected 2D var");
    }

    size_t ny = 0, nx = 0;
    if (nc_inq_dimlen(ncid, dimids[0], &ny) != NC_NOERR ||
        nc_inq_dimlen(ncid, dimids[1], &nx) != NC_NOERR) {
        nc_close(ncid);
        throw std::runtime_error("nc_inq_dimlen failed");
    }

    std::vector<double> flat(nx * ny);
    if (vtype == NC_DOUBLE) {
        if (nc_get_var_double(ncid, varid, flat.data()) != NC_NOERR) {
            nc_close(ncid);
            throw std::runtime_error("nc_get_var_double failed");
        }
    } else if (vtype == NC_FLOAT) {
        std::vector<float> f(nx * ny);
        if (nc_get_var_float(ncid, varid, f.data()) != NC_NOERR) {
            nc_close(ncid);
            throw std::runtime_error("nc_get_var_float failed");
        }
        for (size_t i = 0; i < f.size(); ++i) flat[i] = (double)f[i];
    } else {
        nc_close(ncid);
        throw std::runtime_error("unsupported var type");
    }

    nc_close(ncid);

    std::vector<std::vector<double>> grid(ny, std::vector<double>(nx));
    for (size_t j = 0; j < ny; ++j) {
        for (size_t i = 0; i < nx; ++i) grid[j][i] = flat[j * nx + i];
    }
    return grid;
}
#endif

static std::vector<RankTile> read_rank_layout(const std::filesystem::path& p) {
    std::ifstream ifs(p);
    if (!ifs)
        throw std::runtime_error("Failed to open " + p.string());
    std::string line;
    if (!std::getline(ifs, line)) {
        throw std::runtime_error("Empty rank layout: " + p.string());
    }

    std::vector<RankTile> tiles;
    while (std::getline(ifs, line)) {
        if (line.empty())
            continue;
        std::stringstream ss(line);
        std::string tok;
        RankTile t{};
        std::getline(ss, tok, ',');
        t.rank = std::stoi(tok);
        std::getline(ss, tok, ',');
        t.x_off = std::stoi(tok);
        std::getline(ss, tok, ',');
        t.y_off = std::stoi(tok);
        std::getline(ss, tok, ',');
        t.nx = std::stoi(tok);
        std::getline(ss, tok, ',');
        t.ny = std::stoi(tok);
        std::getline(ss, tok, ',');
        t.halo = std::stoi(tok);
        std::getline(ss, tok, ',');
        t.nxg = std::stoi(tok);
        std::getline(ss, tok, ',');
        t.nyg = std::stoi(tok);
        tiles.push_back(t);
    }
    return tiles;
}

static std::vector<std::vector<double>> assemble_global_csv_snapshot(int step) {
    auto tiles = read_rank_layout("outputs/rank_layout.csv");
    if (tiles.empty())
        throw std::runtime_error("rank_layout.csv has no tiles");

    const int NX = tiles[0].nxg;
    const int NY = tiles[0].nyg;
    std::vector<std::vector<double>> U(NY, std::vector<double>(NX, 0.0));

    for (const auto& t : tiles) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "snapshot_%05d_rank%05d.csv", step, t.rank);
        fs::path f = fs::path("outputs/snapshots") / buf;
        if (!fs::exists(f))
            throw std::runtime_error("Missing snapshot: " + f.string());

        auto tile = read_csv_2d(f);
        if (tile.empty())
            throw std::runtime_error("Empty tile: " + f.string());

        const int ny_csv = static_cast<int>(tile.size());
        const int nx_csv = static_cast<int>(tile[0].size());
        const int expect_ny = t.ny + 2 * t.halo;
        const int expect_nx = t.nx + 2 * t.halo;

        if (ny_csv != expect_ny || nx_csv != expect_nx) {
            std::ostringstream oss;
            oss << "Tile size mismatch for rank " << t.rank << " got (" << ny_csv << "," << nx_csv
                << ")"
                << " expected (" << expect_ny << "," << expect_nx << ")";
            throw std::runtime_error(oss.str());
        }

        for (int j = 0; j < t.ny; ++j) {
            for (int i = 0; i < t.nx; ++i) {
                U[t.y_off + j][t.x_off + i] = tile[j + t.halo][i + t.halo];
            }
        }
    }
    return U;
}

static double sum2d(const std::vector<std::vector<double>>& A) {
    double s = 0.0;
    for (auto& r : A)
        for (double v : r) s += v;
    return s;
}

static double com_x(const std::vector<std::vector<double>>& A) {
    int NY = (int)A.size(), NX = (int)A[0].size();
    double m = 0.0, sx = 0.0;
    for (int j = 0; j < NY; ++j)
        for (int i = 0; i < NX; ++i) {
            m += A[j][i];
            sx += A[j][i] * (i + 0.5);
        }
    return sx / std::max(m, 1e-300);
}

static int run_cmd(const std::string& cmd) { return std::system(cmd.c_str()); }

// ---------- Tests ----------
TEST(Integration_Main, ConstantZeroCSV_Step0_AllZeros) {
    std::ostringstream cmd;
    cmd << MPIEXEC_EXECUTABLE << " " << MPIEXEC_NUMPROC_FLAG << " 2 " << CLIMATE_SIM_EXE
        << " --nx=32 --ny=32 --dx=1 --dy=1"
        << " --D=0 --vx=0 --vy=0"
        << " --dt=0.1 --steps=1 --out_every=1"
        << " --bc=periodic"
        << " --ic.mode=preset --ic.preset=constant_zero";

    ASSERT_EQ(run_cmd(cmd.str()), 0) << "main run failed";

    fs::path snap = fs::path("outputs/snapshots") / "snapshot_00000_rank00000.csv";
    ASSERT_TRUE(fs::exists(snap)) << "missing " << snap;

    auto grid = read_csv_2d(snap);
    ASSERT_FALSE(grid.empty());
    for (const auto& row : grid) {
        for (double v : row) {
            EXPECT_DOUBLE_EQ(v, 0.0);
        }
    }
}

TEST(Integration_Main, GaussianPresetCSV_NontrivialRange) {
    fs::remove_all("outputs");
    fs::create_directories("outputs/snapshots");

    std::ostringstream cmd;
    cmd << MPIEXEC_EXECUTABLE << " " << MPIEXEC_NUMPROC_FLAG << " 2 " << CLIMATE_SIM_EXE
        << " --nx=64 --ny=64 --dx=1 --dy=1"
        << " --D=0 --vx=0 --vy=0"
        << " --dt=0.1 --steps=1 --out_every=1"
        << " --bc=periodic"
        << " --ic.mode=preset --ic.preset=gaussian_hotspot --ic.A=1.0 --ic.sigma_frac=0.1";
    ASSERT_EQ(run_cmd(cmd.str()), 0);

    auto grid = assemble_global_csv_snapshot(0);
    ASSERT_FALSE(grid.empty());
    ASSERT_EQ(grid.size(), 64u);
    ASSERT_EQ(grid[0].size(), 64u);

    double mn = 1e300, mx = -1e300;
    for (const auto& row : grid)
        for (double v : row) {
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }

    EXPECT_GE(mn, 0.0);
    EXPECT_LE(mx, 1.0);
    EXPECT_GT(mx, 1e-6);
}

TEST(Integration_Main, BinaryIC_LoadsCorrectMinMax) {
    fs::remove_all("inputs");
    fs::create_directories("inputs");
    std::ostringstream py;
    py << PYTHON_EXECUTABLE << " " << (fs::path(SCRIPTS_DIR) / "generate_ic.py").string()
       << " --nx 64 --ny 32 --amp 1.0 --sigma-frac 0.1 --fmt bin --out inputs/ic_global";
    ASSERT_EQ(run_cmd(py.str()), 0);

    auto plane = read_bin_plane("inputs/ic_global.bin", 64, 32);
    ASSERT_EQ(plane.size(), 64u * 32u);
    double mx = -1e300;
    for (double v : plane) mx = std::max(mx, v);
    ASSERT_GT(mx, 1e-6);

    fs::remove_all("outputs");
    std::ostringstream cmd;
    cmd << MPIEXEC_EXECUTABLE << " " << MPIEXEC_NUMPROC_FLAG << " 2 " << CLIMATE_SIM_EXE
        << " --nx=64 --ny=32 --dx=1 --dy=1"
        << " --D=0 --vx=0 --vy=0"
        << " --dt=0.1 --steps=1 --out_every=1"
        << " --bc=periodic"
        << " --ic.mode=file --ic.format=bin --ic.path=inputs/ic_global.bin";
    ASSERT_EQ(run_cmd(cmd.str()), 0);

    auto full = assemble_global_csv_snapshot(0);
    ASSERT_EQ(full.size(), 32u);
    ASSERT_EQ(full[0].size(), 64u);

    double mx_snap = -1e300;
    for (auto& r : full)
        for (double v : r) mx_snap = std::max(mx_snap, v);

    EXPECT_NEAR(mx_snap, mx, 1e-3);
}

#ifdef HAS_NETCDF
TEST(Integration_Main, NetCDFOutput_WritesAndIsReadable) {
    fs::remove_all("outputs");

    std::ostringstream cmd;
    cmd << MPIEXEC_EXECUTABLE << " " << MPIEXEC_NUMPROC_FLAG << " 2 " << CLIMATE_SIM_EXE
        << " --nx=32 --ny=32 --dx=1 --dy=1"
        << " --D=0 --vx=0 --vy=0"
        << " --dt=0.1 --steps=1 --out_every=1"
        << " --bc=periodic"
        << " --ic.mode=preset --ic.preset=gaussian_hotspot"
        << " --output.format=netcdf";
    ASSERT_EQ(run_cmd(cmd.str()), 0);

    fs::path nc = fs::path("outputs/snapshots") / "snapshot_00000_rank00000.nc";
    ASSERT_TRUE(fs::exists(nc)) << "NetCDF snapshot missing";

    auto grid = read_nc_2d(nc, "u");
    ASSERT_FALSE(grid.empty());
    ASSERT_FALSE(grid[0].empty());

    double mn = 1e300, mx = -1e300;
    for (auto& r : grid)
        for (double v : r) {
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }
    EXPECT_GE(mn, 0.0);
    EXPECT_LE(mx, 1.0);
    EXPECT_GT(mx, 1e-6);
}
#endif

TEST(Integration_Main, Diffusion_DecreasesPeak) {
    fs::remove_all("outputs");
    fs::create_directories("outputs/snapshots");

    std::ostringstream cmd;
    cmd << MPIEXEC_EXECUTABLE << " " << MPIEXEC_NUMPROC_FLAG << " 2 " << CLIMATE_SIM_EXE
        << " --nx=64 --ny=64 --dx=1 --dy=1"
        << " --D=1.0 --vx=0 --vy=0"
        << " --dt=0.1 --steps=11 --out_every=10"
        << " --bc=periodic"
        << " --ic.mode=preset --ic.preset=gaussian_hotspot --ic.A=1.0 --ic.sigma_frac=0.1";
    ASSERT_EQ(run_cmd(cmd.str()), 0);

    fs::path snap0_r0 = fs::path("outputs/snapshots") / "snapshot_00000_rank00000.csv";
    fs::path snap10_r0 = fs::path("outputs/snapshots") / "snapshot_00010_rank00000.csv";
    ASSERT_TRUE(fs::exists(snap0_r0));
    ASSERT_TRUE(fs::exists(snap10_r0));

    auto U0 = assemble_global_csv_snapshot(0);
    auto U10 = assemble_global_csv_snapshot(10);
    ASSERT_FALSE(U0.empty());
    ASSERT_FALSE(U10.empty());
    ASSERT_EQ(U0.size(), 64u);
    ASSERT_EQ(U0[0].size(), 64u);
    ASSERT_EQ(U10.size(), 64u);
    ASSERT_EQ(U10[0].size(), 64u);

    auto peak = [](const auto& G) {
        double mx = -1e300;
        for (const auto& row : G)
            for (double v : row) mx = std::max(mx, v);
        return mx;
    };
    double mx0 = peak(U0);
    double mx10 = peak(U10);
    EXPECT_LT(mx10, mx0);

    for (const auto& row : U10)
        for (double v : row) EXPECT_GE(v, 0.0);
}

TEST(Integration_Main, Advection_ShiftsHotspotRight) {
    fs::remove_all("outputs");

    std::ostringstream cmd;
    cmd << MPIEXEC_EXECUTABLE << " " << MPIEXEC_NUMPROC_FLAG << " 2 " << CLIMATE_SIM_EXE
        << " --nx=64 --ny=64 --dx=1 --dy=1"
        << " --D=0 --vx=1 --vy=0"
        << " --dt=1 --steps=6 --out_every=1"
        << " --bc=periodic"
        << " --ic.mode=preset --ic.preset=gaussian_hotspot --ic.sigma_frac=0.1 --ic.A=1.0";
    ASSERT_EQ(run_cmd(cmd.str()), 0);

    auto U0 = assemble_global_csv_snapshot(0);
    auto U5 = assemble_global_csv_snapshot(5);

    double x0 = com_x(U0);
    double x5 = com_x(U5);
    EXPECT_NEAR((x5 - x0), 5.0, 1.0);

    double s0 = sum2d(U0);
    double s5 = sum2d(U5);
    EXPECT_NEAR(s5, s0, 0.05 * s0);
}
