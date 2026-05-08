#include "observability/slo.h"

#include <utility>

namespace streamrelay::observability {

void SloRegistry::define(SloTarget target) {
    targets_[target.name] = std::move(target);
}

void SloRegistry::measure(std::string name, SloMeasurement measurement) {
    measurements_[std::move(name)] = measurement;
}

bool SloRegistry::passing(const std::string& name) const {
    const auto target = targets_.find(name);
    const auto measurement = measurements_.find(name);
    if (target == targets_.end() || measurement == measurements_.end()) {
        return false;
    }
    return measurement->second.p95_ms <= target->second.target_p95_ms && measurement->second.error_rate <= target->second.max_error_rate;
}

std::size_t SloRegistry::target_count() const noexcept {
    return targets_.size();
}

} 
