#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace streamrelay::observability {

struct SloTarget {
    std::string name;
    std::uint64_t target_p95_ms{0};
    double max_error_rate{0.0};
};

struct SloMeasurement {
    std::uint64_t p95_ms{0};
    double error_rate{0.0};
};

class SloRegistry {
public:
    void define(SloTarget target);
    void measure(std::string name, SloMeasurement measurement);
    bool passing(const std::string& name) const;
    std::size_t target_count() const noexcept;

private:
    std::unordered_map<std::string, SloTarget> targets_;
    std::unordered_map<std::string, SloMeasurement> measurements_;
};

} 
