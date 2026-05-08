#include "observability/health.h"

#include <sstream>
#include <utility>

namespace streamrelay::observability {

void HealthRegistry::report(HealthCheckResult result) {
    checks_[result.name] = std::move(result);
}

HealthState HealthRegistry::health() const noexcept {
    for (const auto& item : checks_) {
        if (item.second.state == HealthState::Failing) {
            return HealthState::Failing;
        }
    }
    for (const auto& item : checks_) {
        if (item.second.state == HealthState::Degraded) {
            return HealthState::Degraded;
        }
    }
    return HealthState::Passing;
}

HealthState HealthRegistry::readiness() const noexcept {
    return health() == HealthState::Failing ? HealthState::Failing : HealthState::Passing;
}

std::vector<HealthCheckResult> HealthRegistry::checks() const {
    std::vector<HealthCheckResult> out;
    out.reserve(checks_.size());
    for (const auto& item : checks_) {
        out.push_back(item.second);
    }
    return out;
}

std::string HealthRegistry::render_text() const {
    std::ostringstream out;
    for (const auto& item : checks_) {
        out << item.second.name << '=';
        if (item.second.state == HealthState::Passing) {
            out << "passing";
        } else if (item.second.state == HealthState::Degraded) {
            out << "degraded";
        } else {
            out << "failing";
        }
        if (!item.second.detail.empty()) {
            out << ' ' << item.second.detail;
        }
        out << '\n';
    }
    return out.str();
}

} 
