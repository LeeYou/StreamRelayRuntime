#include "gateway/gateway_edge.h"

namespace streamrelay::gateway {

GatewayEdge::GatewayEdge(transport::InMemoryTransportServer& transport,
                         GatewayRuntime& runtime,
                         session::InMemorySessionService& sessions,
                         device_registry::InMemoryDeviceRegistry& devices,
                         security::StaticTokenAuthenticator& authenticator,
                         observability::MetricsRegistry& metrics)
    : transport_(transport), runtime_(runtime), sessions_(sessions), devices_(devices), authenticator_(authenticator), metrics_(metrics) {}

core::Result<GatewayEdgeAcceptResult> GatewayEdge::accept_admin(const std::string& token, std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now) {
    auto claims = authenticator_.authenticate(token, system_now);
    if (!claims.ok()) {
        metrics_.increment_counter("gateway.auth_failed");
        return claims.error();
    }

    auto transport_id = transport_.accept(transport::TransportClientKind::Admin, "admin");
    if (!transport_id.ok()) {
        return transport_id.error();
    }

    auto connection = runtime_.accept_admin("websocket", "admin", steady_now);
    if (!connection.ok()) {
        return connection.error();
    }

    session::CreateSessionRequest request;
    request.tenant_id = claims.value().tenant_id;
    request.principal_id = claims.value().principal_id;
    auto session_id = sessions_.create_user_session(request, system_now);
    if (!session_id.ok()) {
        return session_id.error();
    }
    auto bind_result = sessions_.bind_connection(session_id.value(), connection.value(), system_now);
    if (!bind_result.ok()) {
        return bind_result.error();
    }

    bindings_[transport_id.value()] = connection.value();
    metrics_.increment_counter("gateway.admin_accepted");
    return GatewayEdgeAcceptResult{transport_id.value(), connection.value(), session_id.value()};
}

core::Result<GatewayEdgeAcceptResult> GatewayEdge::accept_device(const std::string& token, const std::string& device_id, std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now) {
    auto claims = authenticator_.authenticate(token, system_now);
    if (!claims.ok()) {
        metrics_.increment_counter("gateway.auth_failed");
        return claims.error();
    }

    auto transport_id = transport_.accept(transport::TransportClientKind::DeviceAgent, "device");
    if (!transport_id.ok()) {
        return transport_id.error();
    }

    auto connection = runtime_.accept_device_agent("websocket", "device", steady_now);
    if (!connection.ok()) {
        return connection.error();
    }

    session::CreateSessionRequest request;
    request.tenant_id = claims.value().tenant_id;
    request.principal_id = device_id;
    auto session_id = sessions_.create_device_session(request, system_now);
    if (!session_id.ok()) {
        return session_id.error();
    }
    auto bind_result = sessions_.bind_connection(session_id.value(), connection.value(), system_now);
    if (!bind_result.ok()) {
        return bind_result.error();
    }

    device_registry::DeviceMetadata metadata;
    metadata.tenant_id = claims.value().tenant_id;
    metadata.device_id = device_id;
    metadata.owner_user_id = claims.value().principal_id;
    auto register_result = devices_.register_device(metadata);
    if (!register_result.ok()) {
        return register_result.error();
    }

    device_registry::DeviceHeartbeat heartbeat;
    heartbeat.tenant_id = claims.value().tenant_id;
    heartbeat.device_id = device_id;
    heartbeat.session_id = session_id.value();
    heartbeat.connection = connection.value();
    auto heartbeat_result = devices_.update_heartbeat(heartbeat, system_now);
    if (!heartbeat_result.ok()) {
        return heartbeat_result.error();
    }

    bindings_[transport_id.value()] = connection.value();
    metrics_.increment_counter("gateway.device_accepted");
    return GatewayEdgeAcceptResult{transport_id.value(), connection.value(), session_id.value()};
}

core::Result<void> GatewayEdge::process_next_frame(transport::TransportConnectionId transport_id, std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now) {
    auto connection = connection_for(transport_id);
    if (!connection.ok()) {
        return connection.error();
    }

    auto encoded = transport_.pop_received(transport_id);
    if (!encoded.ok()) {
        return encoded.error();
    }

    auto ws = protocol::decode_websocket_frame(encoded.value(), protocol::WebSocketFrameLimits{});
    if (!ws.ok()) {
        metrics_.increment_counter("gateway.frame_rejected");
        return ws.error();
    }

    auto result = runtime_.receive_frame(connection.value(), ws.value().payload, protocol::to_unix_ms(system_now), steady_now);
    if (!result.ok()) {
        metrics_.increment_counter("gateway.frame_rejected");
        return result;
    }

    metrics_.increment_counter("gateway.frame_processed");
    return core::success();
}

core::Result<void> GatewayEdge::send_websocket_binary(transport::TransportConnectionId transport_id, core::ByteBuffer payload) {
    protocol::WebSocketFrame frame;
    frame.masked = false;
    frame.payload = std::move(payload);
    auto encoded = protocol::encode_websocket_frame(frame);
    if (!encoded.ok()) {
        return encoded.error();
    }
    return transport_.send_to_client(transport_id, encoded.value());
}

core::Result<core::ConnectionRef> GatewayEdge::connection_for(transport::TransportConnectionId transport_id) const {
    const auto it = bindings_.find(transport_id);
    if (it == bindings_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "gateway edge binding not found");
    }
    return it->second;
}

} 
