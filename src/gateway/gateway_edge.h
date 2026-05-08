#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

#include "core/result.h"
#include "device_registry/device_registry.h"
#include "gateway/gateway_runtime.h"
#include "observability/metrics.h"
#include "protocol/websocket_frame.h"
#include "security/auth.h"
#include "session/session_service.h"
#include "transport/in_memory_transport.h"

namespace streamrelay::gateway {

struct GatewayEdgeAcceptResult {
    transport::TransportConnectionId transport_id{0};
    core::ConnectionRef connection;
    std::string session_id;
};

class GatewayEdge {
public:
    GatewayEdge(transport::InMemoryTransportServer& transport,
                GatewayRuntime& runtime,
                session::InMemorySessionService& sessions,
                device_registry::InMemoryDeviceRegistry& devices,
                security::StaticTokenAuthenticator& authenticator,
                observability::MetricsRegistry& metrics);

    core::Result<GatewayEdgeAcceptResult> accept_admin(const std::string& token, std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now);
    core::Result<GatewayEdgeAcceptResult> accept_device(const std::string& token, const std::string& device_id, std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now);
    core::Result<void> receive_websocket_frame(transport::TransportConnectionId transport_id, core::ByteBuffer encoded_frame, std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now);
    core::Result<void> process_next_frame(transport::TransportConnectionId transport_id, std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now);
    core::Result<void> send_websocket_binary(transport::TransportConnectionId transport_id, core::ByteBuffer payload);
    core::Result<core::ConnectionRef> connection_for(transport::TransportConnectionId transport_id) const;

private:
    transport::InMemoryTransportServer& transport_;
    GatewayRuntime& runtime_;
    session::InMemorySessionService& sessions_;
    device_registry::InMemoryDeviceRegistry& devices_;
    security::StaticTokenAuthenticator& authenticator_;
    observability::MetricsRegistry& metrics_;
    std::unordered_map<transport::TransportConnectionId, core::ConnectionRef> bindings_;
};

} 
