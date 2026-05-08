#include <cassert>
#include <chrono>

#include "gateway/gateway_edge.h"
#include "messaging/in_memory_message_bus.h"
#include "observability/metrics.h"
#include "protocol/binary_codec.h"
#include "protocol/websocket_frame.h"
#include "relay/gateway_tunnel_bridge.h"
#include "relay/local_relay_data_plane.h"
#include "control/secure_control_service.h"
#include "security/auth.h"
#include "transport/in_memory_transport.h"

namespace {

streamrelay::core::ByteBuffer make_masked_websocket_envelope(const streamrelay::protocol::Envelope& envelope) {
    auto encoded_envelope = streamrelay::protocol::encode_envelope_frame(envelope);
    assert(encoded_envelope.ok());
    streamrelay::protocol::WebSocketFrame websocket;
    websocket.masked = true;
    websocket.masking_key = 0x01020304;
    websocket.payload = encoded_envelope.value();
    auto encoded_websocket = streamrelay::protocol::encode_websocket_frame(websocket);
    assert(encoded_websocket.ok());
    return encoded_websocket.value();
}

} 

int main() {
    const auto system_now = std::chrono::system_clock::now();
    const auto steady_now = std::chrono::steady_clock::now();

    streamrelay::security::StaticTokenAuthenticator authenticator;
    streamrelay::security::AuthClaims admin_claims;
    admin_claims.tenant_id = "tenant-1";
    admin_claims.principal_id = "operator-1";
    admin_claims.roles.insert("admin");
    admin_claims.expires_at = system_now + std::chrono::hours{1};
    assert(authenticator.add_token("admin-token", admin_claims).ok());

    streamrelay::security::AuthorizationPolicy policy;
    policy.allow_role("admin", "command", "submit");
    assert(policy.authorize(streamrelay::security::AuthorizationRequest{admin_claims, "command", "submit"}).ok());
    auto denied = policy.authorize(streamrelay::security::AuthorizationRequest{admin_claims, "relay", "open"});
    assert(!denied.ok());

    streamrelay::transport::InMemoryTransportServer transport;
    streamrelay::messaging::InMemoryMessageBus bus;
    streamrelay::gateway::GatewayRuntimeOptions options;
    options.connection_manager.gateway_id = "gateway-a";
    streamrelay::gateway::GatewayRuntime runtime{options, bus};
    streamrelay::session::InMemorySessionService sessions;
    streamrelay::device_registry::InMemoryDeviceRegistry devices;
    streamrelay::observability::MetricsRegistry metrics;
    streamrelay::gateway::GatewayEdge edge{transport, runtime, sessions, devices, authenticator, metrics};

    int delivered = 0;
    assert(bus.subscribe(streamrelay::messaging::make_route("control", "submit_command"), [&](const streamrelay::messaging::Message& message) {
        ++delivered;
        assert(message.service == "control");
        assert(message.method == "submit_command");
        return streamrelay::core::success();
    }).ok());

    auto accepted = edge.accept_admin("admin-token", system_now, steady_now);
    assert(accepted.ok());
    assert(metrics.counter("gateway.admin_accepted") == 1);

    streamrelay::protocol::Envelope envelope;
    envelope.request_id = 100;
    envelope.trace_id = 200;
    envelope.service = "control";
    envelope.method = "submit_command";
    envelope.deadline_unix_ms = streamrelay::protocol::to_unix_ms(system_now + std::chrono::seconds{10});
    envelope.payload = streamrelay::core::ByteBuffer{1, 2, 3};
    assert(transport.receive_from_client(accepted.value().transport_id, make_masked_websocket_envelope(envelope)).ok());
    assert(edge.process_next_frame(accepted.value().transport_id, system_now, steady_now).ok());
    assert(delivered == 1);
    assert(metrics.counter("gateway.frame_processed") == 1);

    streamrelay::relay::GatewayTunnelManager gateway_a;
    streamrelay::relay::GatewayTunnelManager gateway_b;
    assert(gateway_a.connect("gateway-b").ok());
    assert(gateway_b.connect("gateway-a").ok());
    streamrelay::relay::RelayFrame frame;
    frame.header.channel_handle = 1;
    frame.payload = streamrelay::core::ByteBuffer{7, 8, 9};
    assert(gateway_a.send_frame("gateway-b", frame).ok());
    assert(streamrelay::relay::GatewayTunnelBridge::pump_once(gateway_a, "gateway-b", gateway_b, "gateway-a", streamrelay::relay::RelayFrameLimits{}).ok());
    auto inbound = gateway_b.pop_inbound_frame("gateway-a");
    assert(inbound.ok());
    assert(inbound.value().payload == frame.payload);

    streamrelay::control::InMemoryCommandStore command_store;
    streamrelay::control::InMemoryCommandAuditLog command_audit;
    streamrelay::control::ControlService control{command_store, command_audit, devices, bus};
    control.allow_command_type("safe.command");
    streamrelay::control::SecureControlService secure_control{control, authenticator, policy};

    streamrelay::control::SecureSubmitCommandRequest secure_request;
    secure_request.token = "admin-token";
    secure_request.command.device_id = "device-1";
    secure_request.command.command_type = "safe.command";
    auto secure_submitted = secure_control.submit_command(secure_request, system_now);
    assert(secure_submitted.ok());

    streamrelay::security::AuthClaims viewer_claims;
    viewer_claims.tenant_id = "tenant-1";
    viewer_claims.principal_id = "viewer-1";
    viewer_claims.roles.insert("viewer");
    viewer_claims.expires_at = system_now + std::chrono::hours{1};
    assert(authenticator.add_token("viewer-token", viewer_claims).ok());
    secure_request.token = "viewer-token";
    auto secure_denied = secure_control.submit_command(secure_request, system_now);
    assert(!secure_denied.ok());

    return 0;
}
