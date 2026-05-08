#include "observability/trace.h"

#include <utility>

namespace streamrelay::observability {

TraceContext InMemoryTracer::start_span(std::uint64_t trace_id, std::string service, std::string operation, std::uint64_t parent_span_id) {
    TraceContext context;
    context.trace_id = trace_id;
    context.span_id = next_span_id_++;
    context.parent_span_id = parent_span_id;
    context.service = std::move(service);
    context.operation = std::move(operation);
    spans_.push_back(TraceSpan{context, false});
    return context;
}

void InMemoryTracer::finish_span(std::uint64_t span_id) {
    for (auto& span : spans_) {
        if (span.context.span_id == span_id) {
            span.ended = true;
            return;
        }
    }
}

const std::vector<TraceSpan>& InMemoryTracer::spans() const noexcept {
    return spans_;
}

} 
