#pragma once

#include <cmath>
#include <cstddef>

namespace stencil_math {
// Unusual spacings can overflow or underflow a double intermediate even when
// the normalized coefficient is finite.
inline double diffusion(double dt, double diffusivity, double spacing) {
    if (diffusivity == 0)
        return 0;
    const double numerator = dt * diffusivity, square = spacing * spacing;
    if (!std::isnormal(square) || !std::isnormal(numerator))
        return static_cast<double>((static_cast<long double>(dt) * diffusivity / spacing) /
                                   spacing);
    return numerator / square;
}

#if defined(__CUDACC__) || defined(__HIPCC__)
#define CLIMATE_INLINE __host__ __device__ inline
#else
#define CLIMATE_INLINE inline
#endif
struct Weights {
    double low, high;
};

CLIMATE_INLINE Weights upwind(double diffusion, double courant) {
    return {diffusion + (courant < 0.0 ? 0.0 : courant),
            diffusion - (0.0 < courant ? 0.0 : courant)};
}

CLIMATE_INLINE double difference(const double* cell, std::ptrdiff_t stride, Weights w) {
    return w.low * (cell[-stride] - *cell) + w.high * (cell[stride] - *cell);
}

CLIMATE_INLINE double interpolate(double lo, double hi, double a) {
    return lo == hi ? lo : (1.0 - a) * lo + a * hi;
}
#undef CLIMATE_INLINE
}  // namespace stencil_math
