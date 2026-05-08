#include "observability/metrics.h"

#include <utility>

namespace streamrelay::observability {

void MetricsRegistry::increment_counter(const std::string& name, std::uint64_t delta) {
    counters_[name] += delta;
}

void MetricsRegistry::set_gauge(const std::string& name, std::int64_t value) {
    gauges_[name] = value;
}

std::uint64_t MetricsRegistry::counter(const std::string& name) const noexcept {
    const auto it = counters_.find(name);
    return it == counters_.end() ? 0 : it->second;
}

std::int64_t MetricsRegistry::gauge(const std::string& name) const noexcept {
    const auto it = gauges_.find(name);
    return it == gauges_.end() ? 0 : it->second;
}

const std::unordered_map<std::string, std::uint64_t>& MetricsRegistry::counters() const noexcept {
    return counters_;
}

const std::unordered_map<std::string, std::int64_t>& MetricsRegistry::gauges() const noexcept {
    return gauges_;
}

void InMemoryLogSink::append(LogEvent event) {
    events_.push_back(std::move(event));
}

const std::vector<LogEvent>& InMemoryLogSink::events() const noexcept {
    return events_;
}

} 
