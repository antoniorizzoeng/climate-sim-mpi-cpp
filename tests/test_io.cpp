#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "io.hpp"

static std::string cfg_path(const char* fname) {
#ifdef CONFIGS_DIR
    return std::string(CONFIGS_DIR) + "/" + fname;
#else
    return fname;
#endif
}

TEST(IO_Yaml, LoadsNestedBlocksAndBC) {
    SimConfig cfg = load_yaml_file(cfg_path("dev.yaml"));
    EXPECT_GT(cfg.nx, 0);
    EXPECT_GT(cfg.ny, 0);
    EXPECT_GT(cfg.dt, 0.0);
    EXPECT_EQ(bc_to_string(cfg.bc), "periodic");
    EXPECT_GE(cfg.D, 0.0);
}

/*TEST(IO_CLI, SimpleScalarOverrides) {
    SimConfig base = load_yaml_file(cfg_path("dev.yaml"));
    std::vector<std::string> args = {
        "--nx=123", "--ny", "321", "--dx=2.5", "--D=0.7", "--vx=1.25",
        "--dt", "0.02", "--bc=dirichlet", "--output_prefix", "unit"
    };
    SimConfig cli = parse_cli_overrides(args);
    SimConfig merged = base;
    merged = merged_config(cfg_path("dev.yaml"), args);

    EXPECT_EQ(merged.nx, 123);
    EXPECT_EQ(merged.ny, 321);
    EXPECT_DOUBLE_EQ(merged.dx, 2.5);
    EXPECT_DOUBLE_EQ(merged.D, 0.7);
    EXPECT_DOUBLE_EQ(merged.vx, 1.25);
    EXPECT_DOUBLE_EQ(merged.dt, 0.02);
    EXPECT_EQ(bc_to_string(merged.bc), "dirichlet");
    EXPECT_EQ(merged.output_prefix, "unit");
}*/

TEST(IO_CLI, ICOverridesTakePrecedence) {
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

TEST(IO_BC, ParseRoundtrip) {
    EXPECT_EQ(bc_from_string("dirichlet"), BCType::Dirichlet);
    EXPECT_EQ(bc_from_string("neumann"), BCType::Neumann);
    EXPECT_EQ(bc_from_string("periodic"), BCType::Periodic);

    EXPECT_EQ(bc_to_string(BCType::Dirichlet), std::string("dirichlet"));
    EXPECT_EQ(bc_to_string(BCType::Neumann), std::string("neumann"));
    EXPECT_EQ(bc_to_string(BCType::Periodic), std::string("periodic"));
}
