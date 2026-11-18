#pragma once
#include <stdexcept>
#include <vector>

#include "decomp.hpp"

struct Field {
    int nx_local, ny_local, nz_local;
    int halo, z_halo;
    double dx, dy, dz;
    std::vector<double> data;

    Field(int nx, int ny, int h, double dx, double dy);
    Field(const Decomposition& dec, int h, double dx, double dy, double dz);

    size_t idx(int i, int j) const;
    double& at(int i, int j);
    const double& at(int i, int j) const;
    size_t idx(int i, int j, int k) const;
    double& at(int i, int j, int k) { return data[idx(i, j, k)]; }
    const double& at(int i, int j, int k) const { return data[idx(i, j, k)]; }

    int nx_total() const { return nx_local + 2 * halo; }
    int ny_total() const { return ny_local + 2 * halo; }
    int nz_total() const { return nz_local + 2 * z_halo; }
    size_t plane_size() const { return static_cast<size_t>(nx_total()) * ny_total(); }
    void fill(double value);

   private:
    Field(int nx, int ny, int nz, int h, int hz, double dx, double dy, double dz);
};
