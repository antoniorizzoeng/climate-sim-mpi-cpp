#pragma once
#include <vector>
#include <stdexcept>

struct Field {
    int nx_local, ny_local;
    int halo;
    double dx, dy;
    std::vector<double> data;

    Field(int nx, int ny, int h, double dx_, double dy_);

    inline size_t idx(int i, int j) const;
    double& at(int i, int j);
    const double& at(int i, int j) const;

    int nx_total() const { return nx_local + 2*halo; }
    int ny_total() const { return ny_local + 2*halo; }

    void fill(double value);
};
