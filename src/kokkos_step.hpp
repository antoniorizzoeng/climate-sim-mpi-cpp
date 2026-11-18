#pragma once

#include <memory>

#include "step.hpp"

class KokkosStep {
   public:
    KokkosStep(const Field& initial, const Decomposition& dec, const SimConfig& cfg);
    ~KokkosStep();
    void advance(const std::optional<WindSample>& wind);
    void stage_output(Field& host) const;
    void fence() const;

   private:
    struct State;
    std::unique_ptr<State> state_;
};
