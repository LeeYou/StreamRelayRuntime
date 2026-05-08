#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace streamrelay::observability {

struct TraceContext {
    std::uint64_t trace_id{0};
    std::uint64_t span_id{0};
    std::uint64_t parent_span_id{0};
    std::string service;
    std::string operation;
};

struct TraceSpan {
    TraceContext context;
    bool ended{false};
};

class InMemoryTracer {
public:
    TraceContext start_span(std::uint64_t trace_id, std::string service, std::string operation, std::uint64_t parent_span_id = 0);
    void finish_span(std::uint64_t span_id);
    const std::vector<TraceSpan>& spans() const noexcept;

private:
    std::uint64_t next_span_id_{1};
    std::vector<TraceSpan> spans_;
};

} 
