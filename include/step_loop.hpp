#pragma once

#include <memory>
#include <optional>

#include "step.hpp"

#ifdef CLIMATE_ENABLE_KOKKOS
class KokkosStep;
#endif

// Owns the active backend's scratch storage for the lifetime of one simulation.
class StepLoop {
   public:
    StepLoop(Field& u, const Decomposition& dec, const SimConfig& cfg);
    ~StepLoop();
    void advance(std::optional<WindSample> wind = std::nullopt);
    void stage_output();
    void fence() const;

   private:
    Field& host_;
    const Decomposition& dec_;
    const SimConfig& cfg_;
    std::optional<Field> scratch_;
#ifdef CLIMATE_ENABLE_KOKKOS
    std::unique_ptr<KokkosStep> device_;
#endif
};
