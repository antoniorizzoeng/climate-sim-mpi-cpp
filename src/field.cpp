#include "field.hpp"

#include <algorithm>
#include <stdexcept>

Field::Field(int nx, int ny, int h, double dx_, double dy_)
    : Field(nx, ny, 1, h, 0, dx_, dy_, 1.0) {}

Field::Field(const Decomposition& dec, int h, double dx, double dy, double dz)
    : Field(dec.nx_local, dec.ny_local, dec.nz_local, h, dec.nz_global > 1 ? h : 0, dx, dy, dz) {}

Field::Field(int nx, int ny, int nz, int h, int hz, double dx_, double dy_, double dz_)
    : nx_local(nx),
      ny_local(ny),
      nz_local(nz),
      halo(h),
      z_halo(hz),
      dx(dx_),
      dy(dy_),
      dz(dz_),
      data() {
    if (nz_total() && plane_size() > data.max_size() / nz_total())
        throw std::length_error("Local field exceeds addressable memory");
    data.resize(plane_size() * nz_total(), 0.0);
}

inline static void check_bounds(int i, int j, int nx_tot, int ny_tot) {
    if (i < 0 || j < 0 || i >= nx_tot || j >= ny_tot) {
        throw std::out_of_range("Field index out of range");
    }
}

size_t Field::idx(int i, int j) const {
    const int nx_tot = nx_total();
    const int ny_tot = ny_total();
    check_bounds(i, j, nx_tot, ny_tot);
    return static_cast<size_t>(j) * nx_tot + i;
}

size_t Field::idx(int i, int j, int k) const {
    if (k < 0 || k >= nz_total())
        throw std::out_of_range("Field index out of range");
    return static_cast<size_t>(k) * plane_size() + idx(i, j);
}

double& Field::at(int i, int j) { return data.at(idx(i, j)); }

const double& Field::at(int i, int j) const { return data.at(idx(i, j)); }

void Field::fill(double value) { std::fill(data.begin(), data.end(), value); }
