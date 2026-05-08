#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace streamrelay::observability {

enum class HealthState {
    Passing,
    Degraded,
    Failing,
};

struct HealthCheckResult {
    std::string name;
    HealthState state{HealthState::Passing};
    std::string detail;
};

class HealthRegistry {
public:
    void report(HealthCheckResult result);
    HealthState health() const noexcept;
    HealthState readiness() const noexcept;
    std::vector<HealthCheckResult> checks() const;
    std::string render_text() const;

private:
    std::unordered_map<std::string, HealthCheckResult> checks_;
};

} 
