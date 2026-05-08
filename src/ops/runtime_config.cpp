#include "ops/runtime_config.h"

namespace streamrelay::ops {

core::Result<void> RuntimeConfigValidator::validate(const RuntimeConfig& config) {
    if (config.environment.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "environment is required");
    }
    if (config.gateway_id.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "gateway_id is required");
    }
    if (config.public_port == 0 || config.internal_port == 0) {
        return core::make_error(core::ErrorCode::InvalidArgument, "ports must be non-zero");
    }
    if (config.public_port == config.internal_port) {
        return core::make_error(core::ErrorCode::InvalidArgument, "public and internal ports must differ");
    }
    if (config.environment == "prod" && config.tls_enabled && config.required_secrets.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "prod tls configuration requires secrets");
    }
    return core::success();
}

} 
