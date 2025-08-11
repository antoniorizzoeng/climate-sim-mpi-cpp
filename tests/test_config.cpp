#include "gtest/gtest.h"
#include "io.hpp"
#include <fstream>

std::string write_temp_yaml(const std::string &content) {
    char filename[] = "/tmp/test_config_XXXXXX.yaml";
    int fd = mkstemps(filename, 5);
    if (fd == -1) {
        throw std::runtime_error("Failed to create temp YAML file");
    }
    write(fd, content.c_str(), content.size());
    close(fd);
    return std::string(filename);
}


TEST(Config, BCStringRoundTrip) {
    EXPECT_EQ(bc_from_string("dirichlet"), BCType::Dirichlet);
    EXPECT_EQ(bc_from_string("neumann"),   BCType::Neumann);
    EXPECT_EQ(bc_from_string("periodic"),  BCType::Periodic);
    EXPECT_EQ(bc_to_string(BCType::Dirichlet), std::string("dirichlet"));
}

TEST(Config, LoadYamlFromFile) {
    std::string path = std::string(CONFIGS_DIR) + "/test.yaml";
    auto cfg = load_yaml_file(path);
    EXPECT_EQ(cfg.nx, 16);
    EXPECT_EQ(cfg.ny, 20);
    EXPECT_DOUBLE_EQ(cfg.D, 0.01);
    EXPECT_EQ(cfg.bc, BCType::Periodic);
}

TEST(Config, CLIOverridesYaml) {
    std::string path = std::string(CONFIGS_DIR) + "/test.yaml";
    std::vector<std::string> cli = {"--nx=32", "--vy", "2.5", "--bc=neumann", "--output_prefix=bench"};
    auto cfg = merged_config(path, cli);
    EXPECT_EQ(cfg.nx, 32);
    EXPECT_EQ(cfg.ny, 20);
    EXPECT_DOUBLE_EQ(cfg.vy, 2.5);
    EXPECT_EQ(cfg.bc, BCType::Neumann);
    EXPECT_EQ(cfg.output_prefix, "bench");
}

TEST(Config, ValidateGuards) {
    SimConfig c;
    c.nx = 0;
    EXPECT_THROW(c.validate(), std::runtime_error);
}
