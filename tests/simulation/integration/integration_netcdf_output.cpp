// integration_test.cpp
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <fstream>
#include <future>

#include "integration_helpers.hpp"

TEST(Integration, NetCDFOutput_WritesAndIsReadable) {
    const TempRunDirectory dir;
    const auto snapshot = dir.path / "new/nested/snapshots.nc";

    std::vector<std::string> args = {"--nx=32",
                                     "--ny=32",
                                     "--dx=1",
                                     "--dy=1",
                                     "--D=0",
                                     "--vx=0",
                                     "--vy=0",
                                     "--dt=0.1",
                                     "--steps=1",
                                     "--out_every=1",
                                     "--bc=periodic",
                                     "--ic.mode=preset",
                                     "--ic.preset=gaussian_hotspot"};
    args.push_back("--output=" + snapshot.string());
    ASSERT_EQ(run_mpi_cmd(CLIMATE_SIM_EXE, args), 0);

    ASSERT_TRUE(fs::exists(snapshot)) << "NetCDF snapshot missing";

    auto grid = read_nc_2d(snapshot, 0, "u");
    ASSERT_EQ(grid.size(), 32u);
    ASSERT_EQ(grid[0].size(), 32u);
    EXPECT_GT(sum2d(grid), 0.0);
}

TEST(Integration, ConcurrentRunsWriteFinalFrameAndProgress) {
    const TempRunDirectory dir;
    const std::vector<std::string> common = {
        "--nx=16", "--ny=16", "--vx=2", "--dt=1", "--out_every=2"};
    const auto regular_snapshot = dir.path / "regular/frames.nc";
    const auto regular_progress = dir.path / "regular/progress.json";
    const auto extra_snapshot = dir.path / "extra/frames.nc";
    const auto extra_progress = dir.path / "extra/progress.json";
    const auto config = dir.path / "run.yaml";
    std::ofstream(config) << "output:\n  path: " << regular_snapshot.string()
                          << "\n  progress: " << regular_progress.string()
                          << "\n  prefix: project\n";

    auto regular = common;
    regular.insert(regular.end(), {"--config=" + config.string(), "--steps=4"});
    auto extra = common;
    extra.insert(extra.end(),
                 {"--config=" + config.string(),
                  "--steps=5",
                  "--output=" + extra_snapshot.string(),
                  "--progress=" + extra_progress.string()});
    auto first = std::async(std::launch::async, run_mpi_cmd, CLIMATE_SIM_EXE, regular);
    auto second = std::async(std::launch::async, run_mpi_cmd, CLIMATE_SIM_EXE, extra);
    ASSERT_EQ(first.get(), 0);
    ASSERT_EQ(second.get(), 0);

    // Steps 0, 2, 4 in the first run and 0, 2, 4, 5 in the second.
    const auto regular_final = read_nc_2d(regular_snapshot, 2, "u");
    const auto extra_final = read_nc_2d(extra_snapshot, 3, "u");
    EXPECT_EQ(regular_final, read_nc_2d(extra_snapshot, 2, "u"));
    EXPECT_NE(extra_final, regular_final);
    EXPECT_THROW(read_nc_2d(regular_snapshot, 3, "u"), std::runtime_error);
    EXPECT_THROW(read_nc_2d(extra_snapshot, 4, "u"), std::runtime_error);

    auto expect_done = [](const fs::path& path, int steps, int frames) {
        const auto state = YAML::LoadFile(path.string());
        EXPECT_EQ(state["step"].as<int>(), steps);
        EXPECT_EQ(state["steps"].as<int>(), steps);
        EXPECT_DOUBLE_EQ(state["time"].as<double>(), steps * 0.5);
        EXPECT_EQ(state["frames"].as<int>(), frames);
        EXPECT_EQ(state["status"].as<std::string>(), "done");
        EXPECT_EQ(state["message"].as<std::string>(), "");
    };
    expect_done(regular_progress, 4, 3);
    expect_done(extra_progress, 5, 4);
}

TEST(Integration, ReportsOutputDirectoryFailureInProgress) {
    const TempRunDirectory dir;
    const auto blocked = dir.path / "file";
    const auto progress = dir.path / "progress.json";
    std::ofstream(blocked) << "prevents directory creation\n";
    const std::vector<std::string> args = {"--nx=16",
                                           "--ny=16",
                                           "--steps=1",
                                           "--output=" + (blocked / "snapshots.nc").string(),
                                           "--progress=" + progress.string()};
    EXPECT_NE(run_mpi_cmd(CLIMATE_SIM_EXE, args), 0);

    const auto state = YAML::LoadFile(progress.string());
    EXPECT_EQ(state["status"].as<std::string>(), "error");
    EXPECT_NE(state["message"].as<std::string>().find(blocked.string()), std::string::npos);
    EXPECT_EQ(state["frames"].as<int>(), 0);
}

TEST(Integration, ReportsInvalidConfigurationToYamlProgressPath) {
    const TempRunDirectory dir;
    const auto config = dir.path / "config.yaml";
    const auto progress = dir.path / "progress.json";
    std::ofstream(config) << "time: {steps: -1}\noutput:\n  progress: " << progress.string()
                          << "\n";
    EXPECT_NE(run_mpi_cmd(CLIMATE_SIM_EXE, {"--config=" + config.string()}), 0);

    const auto state = YAML::LoadFile(progress.string());
    EXPECT_EQ(state["status"].as<std::string>(), "error");
    EXPECT_EQ(state["message"].as<std::string>(), "steps must be > 0");
    EXPECT_EQ(state["frames"].as<int>(), 0);
}
