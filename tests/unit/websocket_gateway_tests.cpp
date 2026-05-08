#include <cassert>
#include <chrono>
#include <string>

#include "gateway/websocket_gateway_adapter.h"
#include "messaging/in_memory_message_bus.h"
#include "observability/metrics.h"
#include "security/auth.h"
#include "transport/websocket_handshake.h"

namespace {

std::string handshake_request(const std::string& token, const std::string& device_id = {}) {
    std::string request;
    request += "GET /streamrelay HTTP/1.1\r\n";
    request += "Host: localhost\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n";
    request += "Sec-WebSocket-Version: 13\r\n";
    request += "X-StreamRelay-Token: " + token + "\r\n";
    if (!device_id.empty()) {
        request += "X-StreamRelay-Device-Id: " + device_id + "\r\n";
    }
    request += "\r\n";
    return request;
}

} 

int main() {
    auto parsed = streamrelay::transport::WebSocketHandshakeCodec::parse_request(handshake_request("token"));
    assert(parsed.ok());
    auto response = streamrelay::transport::WebSocketHandshakeCodec::build_response(parsed.value());
    assert(response.ok());
    assert(response.value().accept_key == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    assert(response.value().raw_response.find("101 Switching Protocols") != std::string::npos);

    auto invalid = streamrelay::transport::WebSocketHandshakeCodec::parse_request("POST / HTTP/1.1\r\n\r\n");
    assert(!invalid.ok());

    const auto system_now = std::chrono::system_clock::now();
    const auto steady_now = std::chrono::steady_clock::now();
    streamrelay::security::StaticTokenAuthenticator authenticator;
    streamrelay::security::AuthClaims claims;
    claims.tenant_id = "tenant-1";
    claims.principal_id = "operator-1";
    claims.roles.insert("admin");
    claims.expires_at = system_now + std::chrono::hours{1};
    assert(authenticator.add_token("admin-token", claims).ok());

    streamrelay::transport::InMemoryTransportServer transport;
    streamrelay::messaging::InMemoryMessageBus bus;
    streamrelay::gateway::GatewayRuntimeOptions options;
    options.connection_manager.gateway_id = "gateway-a";
    streamrelay::gateway::GatewayRuntime runtime{options, bus};
    streamrelay::session::InMemorySessionService sessions;
    streamrelay::device_registry::InMemoryDeviceRegistry devices;
    streamrelay::observability::MetricsRegistry metrics;
    streamrelay::gateway::GatewayEdge edge{transport, runtime, sessions, devices, authenticator, metrics};
    streamrelay::gateway::WebSocketGatewayAdapter adapter{edge};

    auto accepted_admin = adapter.accept_admin_handshake(handshake_request("admin-token"), system_now, steady_now);
    assert(accepted_admin.ok());
    assert(accepted_admin.value().handshake_response.find("Sec-WebSocket-Accept") != std::string::npos);
    assert(metrics.counter("gateway.admin_accepted") == 1);

    auto accepted_device = adapter.accept_device_handshake(handshake_request("admin-token", "device-1"), system_now, steady_now);
    assert(accepted_device.ok());
    auto found = devices.find_device("tenant-1", "device-1", system_now);
    assert(found.ok());
    assert(found.value().presence.connection.has_value());

    auto rejected_device = adapter.accept_device_handshake(handshake_request("admin-token"), system_now, steady_now);
    assert(!rejected_device.ok());

    return 0;
}
