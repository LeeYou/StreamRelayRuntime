#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace streamrelay::observability {

class MetricsRegistry {
public:
    void increment_counter(const std::string& name, std::uint64_t delta = 1);
    void set_gauge(const std::string& name, std::int64_t value);
    std::uint64_t counter(const std::string& name) const noexcept;
    std::int64_t gauge(const std::string& name) const noexcept;
    const std::unordered_map<std::string, std::uint64_t>& counters() const noexcept;
    const std::unordered_map<std::string, std::int64_t>& gauges() const noexcept;

private:
    std::unordered_map<std::string, std::uint64_t> counters_;
    std::unordered_map<std::string, std::int64_t> gauges_;
};

struct LogEvent {
    std::string level;
    std::string event;
    std::string detail;
};

class InMemoryLogSink {
public:
    void append(LogEvent event);
    const std::vector<LogEvent>& events() const noexcept;

private:
    std::vector<LogEvent> events_;
};

} 
