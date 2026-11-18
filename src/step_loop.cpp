#include "step_loop.hpp"

#ifdef CLIMATE_ENABLE_KOKKOS
#include "kokkos_step.hpp"
#endif

StepLoop::StepLoop(Field& u, const Decomposition& dec, const SimConfig& cfg)
    : host_(u), dec_(dec), cfg_(cfg) {
#ifdef CLIMATE_ENABLE_KOKKOS
    if (cfg.backend == "kokkos") {
        device_ = std::make_unique<KokkosStep>(u, dec, cfg);
        return;
    }
#endif
    scratch_.emplace(dec, u.halo, u.dx, u.dy, u.dz);
}

StepLoop::~StepLoop() = default;

void StepLoop::advance(std::optional<WindSample> wind) {
#ifdef CLIMATE_ENABLE_KOKKOS
    if (device_) {
        device_->advance(wind);
        return;
    }
#endif
    if (wind)
        advance_field(host_, *scratch_, dec_, cfg_, *wind);
    else
        advance_field(host_, *scratch_, dec_, cfg_);
}

void StepLoop::stage_output() {
#ifdef CLIMATE_ENABLE_KOKKOS
    if (device_)
        device_->stage_output(host_);
#endif
}

void StepLoop::fence() const {
#ifdef CLIMATE_ENABLE_KOKKOS
    if (device_)
        device_->fence();
#endif
}
