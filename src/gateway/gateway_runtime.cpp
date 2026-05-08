#include "gateway/gateway_runtime.h"

#include <utility>

namespace streamrelay::gateway {

GatewayRuntime::GatewayRuntime(GatewayRuntimeOptions options, messaging::InMemoryMessageBus& message_bus)
    : options_(std::move(options)), message_bus_(&message_bus), connections_(options_.connection_manager) {}

core::Result<core::ConnectionRef> GatewayRuntime::accept_admin(std::string protocol, std::string remote_address, std::chrono::steady_clock::time_point now) {
    return accept(std::move(protocol), std::move(remote_address), ClientKind::Admin, now);
}

core::Result<core::ConnectionRef> GatewayRuntime::accept_device_agent(std::string protocol, std::string remote_address, std::chrono::steady_clock::time_point now) {
    return accept(std::move(protocol), std::move(remote_address), ClientKind::DeviceAgent, now);
}

core::Result<void> GatewayRuntime::receive_frame(core::ConnectionRef ref, const core::ByteBuffer& frame, std::int64_t now_unix_ms, std::chrono::steady_clock::time_point now) {
    auto connection = connections_.find_connection(ref);
    if (!connection.ok()) {
        return connection.error();
    }

    if (connection.value().state == ConnectionState::Closed || connection.value().state == ConnectionState::Closing) {
        return core::make_error(core::ErrorCode::InvalidState, "connection is not active");
    }

    auto decoded = protocol::decode_envelope_frame(frame, options_.frame_limits, now_unix_ms);
    if (!decoded.ok()) {
        return decoded.error();
    }

    messaging::Message message;
    message.request_id = decoded.value().request_id;
    message.trace_id = decoded.value().trace_id;
    message.service = decoded.value().service;
    message.method = decoded.value().method;
    message.payload = decoded.value().payload;

    auto publish_result = message_bus_->publish(message);
    if (!publish_result.ok()) {
        return publish_result;
    }

    return connections_.mark_read(ref, now);
}

ConnectionManager& GatewayRuntime::connections() noexcept {
    return connections_;
}

const ConnectionManager& GatewayRuntime::connections() const noexcept {
    return connections_;
}

core::Result<core::ConnectionRef> GatewayRuntime::accept(std::string protocol, std::string remote_address, ClientKind kind, std::chrono::steady_clock::time_point now) {
    auto ref = connections_.accept_connection(std::move(protocol), std::move(remote_address), now);
    if (!ref.ok()) {
        return ref.error();
    }

    auto bind_result = connections_.bind_connection(ref.value(), kind);
    if (!bind_result.ok()) {
        return bind_result.error();
    }

    return ref.value();
}

} 
