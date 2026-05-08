#include "runtime/app_context.h"

namespace streamrelay::runtime {

AppContext::AppContext(std::string service_name, std::ostream& log_stream)
    : service_name_(std::move(service_name)), log_stream_(&log_stream) {}

const std::string& AppContext::service_name() const noexcept {
    return service_name_;
}

std::ostream& AppContext::log_stream() noexcept {
    return *log_stream_;
}

std::chrono::steady_clock::time_point AppContext::now() const noexcept {
    return std::chrono::steady_clock::now();
}

} // namespace streamrelay::runtime
