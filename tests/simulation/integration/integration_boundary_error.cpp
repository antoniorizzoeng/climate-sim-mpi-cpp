#include <gtest/gtest.h>

#include "integration_helpers.hpp"

TEST(Integration_Main, BoundaryConditions_ErrorHandling) {
    fs::remove_all("outputs");
    fs::create_directory("outputs");

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
    ASSERT_EQ(run_mpi_cmd(CLIMATE_SIM_EXE, args_ok), 0);

    fs::path simfile = "outputs/snapshots.nc";
    ASSERT_TRUE(fs::exists(simfile));

    fs::remove_all("outputs");
    fs::create_directory("outputs");

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
                                            "--ic.path=inputs/does_not_exist.nc"};
    const int ret = run_mpi_cmd(CLIMATE_SIM_EXE, args_bad_ic);
    EXPECT_NE(ret, 0);

    ASSERT_FALSE(fs::exists("outputs/snapshots.nc"));
}