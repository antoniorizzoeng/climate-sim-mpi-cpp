#include "field.hpp"

#include <algorithm>
#include <stdexcept>

Field::Field(int nx, int ny, int h, double dx_, double dy_)
    : nx_local(nx),
      ny_local(ny),
      halo(h),
      dx(dx_),
      dy(dy_),
      data(static_cast<size_t>((nx + 2 * h) * (ny + 2 * h)), 0.0) {}

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

double& Field::at(int i, int j) { return data.at(idx(i, j)); }

const double& Field::at(int i, int j) const { return data.at(idx(i, j)); }

void Field::fill(double value) { std::fill(data.begin(), data.end(), value); }
