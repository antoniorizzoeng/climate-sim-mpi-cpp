#include <gtest/gtest.h>
#include <netcdf.h>
#include <sys/wait.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>

#include "integration_helpers.hpp"

namespace {

constexpr int nx = 7;
constexpr int ny = 5;

void check_nc(int status) {
    if (status != NC_NOERR)
        throw std::runtime_error(nc_strerror(status));
}

class FileIC : public testing::Test {
   protected:
    void SetUp() override {
        auto pattern = (fs::temp_directory_path() / "climate-sim-ic-XXXXXX").string();
        const char* directory = mkdtemp(pattern.data());
        ASSERT_NE(directory, nullptr);
        workdir = directory;
    }

    void TearDown() override {
        if (!workdir.empty())
            fs::remove_all(workdir);
    }

    void write_input(int format,
                     nc_type type = NC_DOUBLE,
                     const char* variable = "u",
                     int rank = 2,
                     double last_value = 2.5) {
        int file, dims[2], var, auxiliary;
        check_nc(nc_create((workdir / "input.nc").c_str(), format, &file));
        check_nc(nc_def_dim(file, "y", ny, &dims[0]));
        check_nc(nc_def_dim(file, "x", nx, &dims[1]));
        check_nc(nc_def_var(file, variable, type, rank, dims, &var));
        check_nc(nc_def_var(file, "auxiliary", NC_DOUBLE, 1, dims, &auxiliary));
        check_nc(nc_enddef(file));
        std::vector<double> values;
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) values.push_back(10 * j - i + 0.25);
        }
        values.back() = last_value;
        if (type == NC_FLOAT) {
            const std::vector<float> floats(values.begin(), values.end());
            check_nc(nc_put_var_float(file, var, floats.data()));
        } else {
            check_nc(nc_put_var_double(file, var, values.data()));
        }
        check_nc(nc_close(file));
    }

    int run(int ranks, const std::string& args) const {
        // Quote the temporary directory and executables for the shell.
        auto quote = [](const std::string& str) {
            std::string quoted = "'";
            for (const char c : str) quoted += c == '\'' ? "'\\''" : std::string(1, c);
            return quoted + "'";
        };
        std::ostringstream cmd;
        cmd << "cd " << quote(workdir.string()) << " && " << quote(MPIEXEC_EXECUTABLE) << " "
            << MPIEXEC_PREFLAGS << " " << MPIEXEC_NUMPROC_FLAG << " " << ranks << " "
            << quote(CLIMATE_SIM_EXE) << " --steps=1 " << args << " > run.log 2>&1";
        return std::system(cmd.str().c_str());
    }

    std::string log() const {
        std::ifstream file(workdir / "run.log");
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    void expect_snapshot(const char* variable) const {
        const auto input = read_nc_2d(workdir / "input.nc", 0, variable);
        const auto snapshot = read_nc_2d(workdir / "outputs/snapshots.nc", 0, "u");
        EXPECT_EQ(snapshot, input);
    }

    void expect_error(const std::string& message, const std::string& overrides = "") const {
        const int status = run(4, "--ic.mode=file --ic.path=input.nc " + overrides);
        const auto output = log();
        ASSERT_NE(status, -1);
        ASSERT_TRUE(WIFEXITED(status)) << output;
        EXPECT_EQ(WEXITSTATUS(status), 1) << output;
        const auto start = output.find("[error] ");
        ASSERT_NE(start, std::string::npos) << output;
        const auto line = output.substr(start, output.find('\n', start) - start);
        EXPECT_NE(line.find(message), std::string::npos) << output;
        EXPECT_EQ(output.find("[error] ", start + 1), std::string::npos) << output;
        EXPECT_FALSE(fs::exists(workdir / "outputs/snapshots.nc"));
    }

    fs::path workdir;
};

TEST_F(FileIC, SnapshotMatchesCdf1OnOneRank) {
    write_input(NC_CLOBBER);
    std::ofstream(workdir / "config.yaml") << "ic: {mode: file, path: input.nc}\n";
    const auto status = run(1, "--config=config.yaml");
    ASSERT_EQ(status, 0) << log();
    expect_snapshot("u");
}

TEST_F(FileIC, SnapshotMatchesCdf2FloatOnTwoRanks) {
    write_input(NC_64BIT_OFFSET, NC_FLOAT, "temperature");
    std::ofstream(workdir / "config.yaml")
        << "grid: {nx: 7, ny: 5}\nic: {mode: file, path: input.nc, var: temperature}\n";
    const auto status = run(2, "--config=config.yaml");
    ASSERT_EQ(status, 0) << log();
    expect_snapshot("temperature");
}

TEST_F(FileIC, SnapshotMatchesCdf5OnFourRanksWithCliOverrides) {
    write_input(NC_64BIT_DATA, NC_DOUBLE, "temperature");
    std::ofstream(workdir / "config.yaml") << "ic: {mode: preset, path: wrong.nc, var: wrong}\n";
    const auto status = run(
        4, "--config=config.yaml --ic.mode file --ic.path=input.nc --ic.var temperature --nx=7");
    ASSERT_EQ(status, 0) << log();
    expect_snapshot("temperature");
}

TEST_F(FileIC, MissingFile) { expect_error("Cannot open initial condition file 'input.nc'"); }

TEST_F(FileIC, UnreadableFile) {
    std::ofstream(workdir / "input.nc") << "CDF\001truncated";
    expect_error("Cannot open initial condition file 'input.nc'");
}

TEST_F(FileIC, NetCdf4File) {
    write_input(NC_NETCDF4);
    expect_error("PnetCDF cannot read NetCDF-4/HDF5; convert to CDF-5");
}

TEST_F(FileIC, MissingVariableListsAvailableVariables) {
    write_input(NC_64BIT_DATA, NC_DOUBLE, "temperature");
    expect_error("'u' not found in 'input.nc'. Available: temperature, auxiliary");
}

TEST_F(FileIC, WrongRank) {
    write_input(NC_64BIT_DATA, NC_DOUBLE, "u", 1);
    expect_error("must be 2D (y, x); got rank 1");
}

TEST_F(FileIC, WrongGridSize) {
    write_input(NC_64BIT_DATA);
    expect_error("size (5, 7) does not match configured grid (ny, nx) = (6, 7)", "--ny=6");
}

TEST_F(FileIC, UnsupportedType) {
    write_input(NC_64BIT_DATA, NC_INT);
    expect_error("must be float or double");
}

TEST_F(FileIC, NaNOnNonRootRank) {
    write_input(NC_64BIT_DATA, NC_DOUBLE, "u", 2, std::numeric_limits<double>::quiet_NaN());
    expect_error("contains non-finite values");
}

TEST_F(FileIC, InfinityOnNonRootRank) {
    write_input(NC_64BIT_DATA, NC_FLOAT, "u", 2, std::numeric_limits<double>::infinity());
    expect_error("contains non-finite values");
}

}  // namespace
