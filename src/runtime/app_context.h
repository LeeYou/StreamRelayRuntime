#pragma once

#include <chrono>
#include <ostream>
#include <string>

namespace streamrelay::runtime {

class AppContext {
public:
    AppContext(std::string service_name, std::ostream& log_stream);

    const std::string& service_name() const noexcept;
    std::ostream& log_stream() noexcept;
    std::chrono::steady_clock::time_point now() const noexcept;

private:
    std::string service_name_;
    std::ostream* log_stream_;
};

} // namespace streamrelay::runtime
