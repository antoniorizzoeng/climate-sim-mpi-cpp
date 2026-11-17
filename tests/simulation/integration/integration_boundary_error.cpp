#include <gtest/gtest.h>
#include <sys/wait.h>
#include <yaml-cpp/yaml.h>

#include "integration_helpers.hpp"

TEST(Integration_Main, BoundaryConditions_ErrorHandling) {
    const TempRunDirectory dir;
    const auto snapshot = dir.path / "snapshots.nc";

    std::vector<std::string> args_ok = {"--nx=16",
                                        "--ny=16",
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
    args_ok.push_back("--output=" + snapshot.string());
    ASSERT_EQ(run_mpi_cmd(CLIMATE_SIM_EXE, args_ok), 0);

    ASSERT_TRUE(fs::exists(snapshot));

    const auto failed_snapshot = dir.path / "failed.nc";
    const auto progress = dir.path / "status/progress.json";

    std::vector<std::string> args_bad_ic = {"--nx=16",
                                            "--ny=16",
                                            "--dx=1",
                                            "--dy=1",
                                            "--D=0",
                                            "--vx=0",
                                            "--vy=0",
                                            "--dt=0.1",
                                            "--steps=1",
                                            "--out_every=1",
                                            "--bc=periodic",
                                            "--ic.mode=file",
                                            "--ic.path=" + (dir.path / "missing.nc").string(),
                                            "--output=" + failed_snapshot.string(),
                                            "--progress=" + progress.string()};
    const int ret = run_mpi_cmd(CLIMATE_SIM_EXE, args_bad_ic);
    ASSERT_NE(ret, -1);
    ASSERT_TRUE(WIFEXITED(ret));
    EXPECT_EQ(WEXITSTATUS(ret), 1);
    ASSERT_FALSE(fs::exists(failed_snapshot));

    const auto state = YAML::LoadFile(progress.string());
    EXPECT_EQ(state["status"].as<std::string>(), "error");
    EXPECT_NE(state["message"].as<std::string>().find("Cannot open initial condition file"),
              std::string::npos);
    EXPECT_EQ(state["step"].as<int>(), 0);
    EXPECT_EQ(state["steps"].as<int>(), 1);
    EXPECT_EQ(state["time"].as<double>(), 0.0);
    EXPECT_EQ(state["frames"].as<int>(), 0);
}