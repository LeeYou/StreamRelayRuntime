#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/result.h"

namespace streamrelay::ops {

struct RuntimeConfig {
    std::string environment{"dev"};
    std::string gateway_id{"gateway-local"};
    std::uint16_t public_port{8080};
    std::uint16_t internal_port{9090};
    bool tls_enabled{false};
    bool metrics_enabled{true};
    std::vector<std::string> required_secrets;
};

class RuntimeConfigValidator {
public:
    static core::Result<void> validate(const RuntimeConfig& config);
};

} 
