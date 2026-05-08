#pragma once

#include <chrono>

#include "core/result.h"
#include "core/types.h"
#include "gateway/connection_manager.h"
#include "messaging/in_memory_message_bus.h"
#include "protocol/binary_codec.h"

namespace streamrelay::gateway {

struct GatewayRuntimeOptions {
    ConnectionManagerOptions connection_manager;
    protocol::FrameLimits frame_limits;
};

class GatewayRuntime {
public:
    GatewayRuntime(GatewayRuntimeOptions options, messaging::InMemoryMessageBus& message_bus);

    core::Result<core::ConnectionRef> accept_admin(std::string protocol, std::string remote_address, std::chrono::steady_clock::time_point now);
    core::Result<core::ConnectionRef> accept_device_agent(std::string protocol, std::string remote_address, std::chrono::steady_clock::time_point now);
    core::Result<void> receive_frame(core::ConnectionRef ref, const core::ByteBuffer& frame, std::int64_t now_unix_ms, std::chrono::steady_clock::time_point now);

    ConnectionManager& connections() noexcept;
    const ConnectionManager& connections() const noexcept;

private:
    core::Result<core::ConnectionRef> accept(std::string protocol, std::string remote_address, ClientKind kind, std::chrono::steady_clock::time_point now);

    GatewayRuntimeOptions options_;
    messaging::InMemoryMessageBus* message_bus_;
    ConnectionManager connections_;
};

} 
