#include <cassert>
#include <chrono>
#include <string>

#include "gateway/gateway_socket_event_loop.h"
#include "messaging/in_memory_message_bus.h"
#include "observability/metrics.h"
#include "protocol/binary_codec.h"
#include "protocol/websocket_frame.h"
#include "security/auth.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace {

std::string admin_handshake_request(const std::string& token) {
    std::string request;
    request += "GET /streamrelay HTTP/1.1\r\n";
    request += "Host: localhost\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n";
    request += "Sec-WebSocket-Version: 13\r\n";
    request += "X-StreamRelay-Token: " + token + "\r\n";
    request += "\r\n";
    return request;
}

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
    streamrelay::security::AuthClaims claims;
    claims.tenant_id = "tenant-1";
    claims.principal_id = "operator-1";
    claims.roles.insert("admin");
    claims.expires_at = system_now + std::chrono::hours{1};
    auto token_added = authenticator.add_token("admin-token", claims);
    assert(token_added.ok());

    streamrelay::transport::InMemoryTransportServer transport;
    streamrelay::messaging::InMemoryMessageBus bus;
    int delivered = 0;
    auto subscribed = bus.subscribe(streamrelay::messaging::make_route("control", "submit_command"), [&](const streamrelay::messaging::Message& message) {
        ++delivered;
        assert(message.service == "control");
        assert(message.method == "submit_command");
        return streamrelay::core::success();
    });
    assert(subscribed.ok());
    streamrelay::gateway::GatewayRuntimeOptions runtime_options;
    runtime_options.connection_manager.gateway_id = "gateway-a";
    streamrelay::gateway::GatewayRuntime runtime{runtime_options, bus};
    streamrelay::session::InMemorySessionService sessions;
    streamrelay::device_registry::InMemoryDeviceRegistry devices;
    streamrelay::observability::MetricsRegistry metrics;
    streamrelay::gateway::GatewayEdge edge{transport, runtime, sessions, devices, authenticator, metrics};
    streamrelay::gateway::WebSocketGatewayAdapter adapter{edge};
    streamrelay::gateway::GatewaySocketEventLoop event_loop{adapter};

    streamrelay::gateway::GatewaySocketEventLoopOptions options;
    options.tcp.bind_address = "127.0.0.1";
    options.tcp.port = 0;
    options.accept_kind = streamrelay::gateway::GatewaySocketAcceptKind::Admin;
    auto started = event_loop.start(options);

#ifdef _WIN32
    assert(started.ok());
    auto status = event_loop.status();
    assert(status.running);
    assert(status.port > 0);
    assert(status.accepted_connections == 0);

    auto client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(client != INVALID_SOCKET);
    DWORD timeout_ms = 2000;
    auto receive_timeout_result = setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    auto send_timeout_result = setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    assert(receive_timeout_result == 0);
    assert(send_timeout_result == 0);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(status.port);
    auto pton_result = inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    assert(pton_result == 1);
    auto connect_result = connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    assert(connect_result != SOCKET_ERROR);

    const auto request = admin_handshake_request("admin-token");
    auto sent_request = send(client, request.data(), static_cast<int>(request.size()), 0);
    assert(sent_request == static_cast<int>(request.size()));

    auto step = event_loop.run_once(system_now, steady_now);
    assert(step.ok());
    assert(step.value().active_connections == 1);
    assert(step.value().gateway.edge.session_id.find("tenant-1:operator-1:") == 0);
    assert(metrics.counter("gateway.admin_accepted") == 1);

    char response[512]{};
    auto received = recv(client, response, sizeof(response), 0);
    assert(received > 0);
    const std::string raw_response{response, static_cast<std::size_t>(received)};
    assert(raw_response.find("101 Switching Protocols") != std::string::npos);
    assert(raw_response.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos);

    status = event_loop.status();
    assert(status.accepted_connections == 1);
    assert(status.failed_accepts == 0);
    assert(status.active_connections == 1);

    streamrelay::protocol::Envelope envelope;
    envelope.request_id = 100;
    envelope.trace_id = 200;
    envelope.service = "control";
    envelope.method = "submit_command";
    envelope.deadline_unix_ms = streamrelay::protocol::to_unix_ms(system_now + std::chrono::seconds{10});
    envelope.payload = streamrelay::core::ByteBuffer{1, 2, 3};
    auto websocket_frame = make_masked_websocket_envelope(envelope);
    auto sent_frame = send(client, reinterpret_cast<const char*>(websocket_frame.data()), static_cast<int>(websocket_frame.size()), 0);
    assert(sent_frame == static_cast<int>(websocket_frame.size()));

    auto pumped = event_loop.pump_frame_once(step.value().gateway.edge.transport_id, system_now, steady_now);
    assert(pumped.ok());
    assert(delivered == 1);
    assert(metrics.counter("gateway.frame_processed") == 1);
    status = event_loop.status();
    assert(status.frames_processed == 1);
    assert(status.failed_frames == 0);

    closesocket(client);
    auto stopped = event_loop.stop();
    assert(stopped.ok());
    status = event_loop.status();
    assert(!status.running);
    assert(status.active_connections == 0);
#else
    assert(!started.ok());
#endif

    return 0;
}
