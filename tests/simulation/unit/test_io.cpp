#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <vector>

#include "decomp.hpp"
#include "field.hpp"
#include "init.hpp"
#include "io.hpp"

static std::string cfg_path(const char* fname) {
#ifdef CONFIGS_DIR
    return std::string(CONFIGS_DIR) + "/" + fname;
#else
    return fname;
#endif
}

TEST(Unit_IO_Yaml, LoadsNestedBlocksAndBC) {
    SimConfig cfg = load_yaml_file(cfg_path("dev.yaml"));
    EXPECT_GT(cfg.nx, 0);
    EXPECT_GT(cfg.ny, 0);
    EXPECT_GT(cfg.dt, 0.0);
    EXPECT_EQ(bc_to_string(cfg.bc), "periodic");
    EXPECT_GE(cfg.D, 0.0);
}

TEST(Unit_IO_CLI, SimpleScalarOverrides) {
    // base config comes from YAML
    auto tmpfile = "tmp_test.yaml";
    {
        std::ofstream ofs(tmpfile);
        ofs << "grid: { nx: 64, ny: 64, dx: 1.0, dy: 1.0 }\n"
            << "physics: { D: 0.01, vx: 0.0, vy: 0.0 }\n"
            << "time: { dt: 0.1, steps: 10, out_every: 5 }\n"
            << "bc: dirichlet\n"
            << "output: { prefix: \"from_yaml\" }\n";
    }

    std::vector<std::string> args = {
        "--nx=128", "--ny=256", "--dt=0.2", "--bc=periodic", "--output_prefix=from_cli"};

    SimConfig merged = merged_config(tmpfile, args);

    EXPECT_EQ(merged.nx, 128);                       // CLI wins
    EXPECT_EQ(merged.ny, 256);                       // CLI wins
    EXPECT_DOUBLE_EQ(merged.dt, 0.2);                // CLI wins
    EXPECT_EQ(bc_to_string(merged.bc), "periodic");  // CLI wins
    EXPECT_EQ(merged.output_prefix, "from_cli");     // CLI wins
}

TEST(Unit_IO_CLI, ICOverridesTakePrecedence) {
    std::vector<std::string> args = {"--ic.mode=preset",
                                     "--ic.preset=constant_zero",
                                     "--ic.A=999.0",
                                     "--ic.sigma_frac=0.25",
                                     "--ic.xc_frac=0.1",
                                     "--ic.yc_frac=0.2"};
    SimConfig merged = merged_config(cfg_path("dev.yaml"), args);

    EXPECT_EQ(merged.ic.mode, "preset");
    EXPECT_EQ(merged.ic.preset, "constant_zero");
    EXPECT_DOUBLE_EQ(merged.ic.A, 999.0);
    EXPECT_DOUBLE_EQ(merged.ic.sigma_frac, 0.25);
    EXPECT_DOUBLE_EQ(merged.ic.xc_frac, 0.1);
    EXPECT_DOUBLE_EQ(merged.ic.yc_frac, 0.2);
}

TEST(Unit_IO_BC, ParseRoundtrip) {
    EXPECT_EQ(bc_from_string("dirichlet"), BCType::Dirichlet);
    EXPECT_EQ(bc_from_string("neumann"), BCType::Neumann);
    EXPECT_EQ(bc_from_string("periodic"), BCType::Periodic);

    EXPECT_EQ(bc_to_string(BCType::Dirichlet), std::string("dirichlet"));
    EXPECT_EQ(bc_to_string(BCType::Neumann), std::string("neumann"));
    EXPECT_EQ(bc_to_string(BCType::Periodic), std::string("periodic"));
}

TEST(Unit_IO_CLI, InvalidBoundaryConditionThrows) {
    std::vector<std::string> args = {"--bc=foobar"};
    EXPECT_THROW({ merged_config(std::nullopt, args); }, std::runtime_error);
}

TEST(Unit_IO_CLI, InvalidGridSizeThrows) {
    std::vector<std::string> args = {"--nx=-10", "--ny=128"};
    EXPECT_THROW({ merged_config(std::nullopt, args); }, std::runtime_error);
}

TEST(Unit_IO_CLI, InvalidTimestepThrows) {
    std::vector<std::string> args = {"--dt=0.0", "--steps=10"};
    EXPECT_THROW({ merged_config(std::nullopt, args); }, std::runtime_error);
}

TEST(Unit_IO_CLI, InvalidICPresetThrows) {
    SimConfig cfg;
    cfg.ic.mode = "preset";
    cfg.ic.preset = "notarealpreset";

    Field f(cfg.nx, cfg.ny, 0, cfg.dx, cfg.dy);
    Decomp2D dec;
    EXPECT_THROW({ apply_initial_condition(dec, f, cfg); }, std::runtime_error);
}
