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

std::string handshake_request(const std::string& token) {
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

streamrelay::protocol::Envelope make_envelope(std::uint64_t request_id, const std::string& method, std::chrono::system_clock::time_point system_now, streamrelay::core::ByteBuffer payload) {
    streamrelay::protocol::Envelope envelope;
    envelope.request_id = request_id;
    envelope.trace_id = request_id + 1000;
    envelope.service = "control";
    envelope.method = method;
    envelope.deadline_unix_ms = streamrelay::protocol::to_unix_ms(system_now + std::chrono::seconds{10});
    envelope.payload = std::move(payload);
    return envelope;
}

#ifdef _WIN32
SOCKET connect_loopback(std::uint16_t port, DWORD timeout_ms) {
    auto client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(client != INVALID_SOCKET);
    auto receive_timeout_result = setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    auto send_timeout_result = setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    assert(receive_timeout_result == 0);
    assert(send_timeout_result == 0);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    auto pton_result = inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    assert(pton_result == 1);
    auto connect_result = connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    assert(connect_result != SOCKET_ERROR);
    return client;
}

void send_all(SOCKET client, const streamrelay::core::ByteBuffer& bytes) {
    auto sent = send(client, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0);
    assert(sent == static_cast<int>(bytes.size()));
}

void send_all(SOCKET client, const std::string& bytes) {
    auto sent = send(client, bytes.data(), static_cast<int>(bytes.size()), 0);
    assert(sent == static_cast<int>(bytes.size()));
}

std::string receive_text(SOCKET client) {
    char buffer[512]{};
    auto received = recv(client, buffer, sizeof(buffer), 0);
    assert(received > 0);
    return std::string{buffer, static_cast<std::size_t>(received)};
}

streamrelay::core::ByteBuffer receive_binary_frame(SOCKET client) {
    char buffer[512]{};
    auto received = recv(client, buffer, sizeof(buffer), 0);
    assert(received > 0);
    return streamrelay::core::ByteBuffer(buffer, buffer + received);
}
#endif

} 

int main() {
    const auto system_now = std::chrono::system_clock::now();
    const auto steady_now = std::chrono::steady_clock::now();

    streamrelay::security::StaticTokenAuthenticator authenticator;
    streamrelay::security::AuthClaims first_claims;
    first_claims.tenant_id = "tenant-1";
    first_claims.principal_id = "operator-1";
    first_claims.roles.insert("admin");
    first_claims.expires_at = system_now + std::chrono::hours{1};
    auto first_token_added = authenticator.add_token("admin-token-1", first_claims);
    assert(first_token_added.ok());
    streamrelay::security::AuthClaims second_claims = first_claims;
    second_claims.principal_id = "operator-2";
    auto second_token_added = authenticator.add_token("admin-token-2", second_claims);
    assert(second_token_added.ok());

    streamrelay::transport::InMemoryTransportServer transport;
    streamrelay::messaging::InMemoryMessageBus bus;
    int first_delivered = 0;
    int second_delivered = 0;
    auto first_subscribed = bus.subscribe(streamrelay::messaging::make_route("control", "submit_first"), [&](const streamrelay::messaging::Message& message) {
        ++first_delivered;
        assert(message.payload == streamrelay::core::ByteBuffer({1, 1, 1}));
        return streamrelay::core::success();
    });
    assert(first_subscribed.ok());
    auto second_subscribed = bus.subscribe(streamrelay::messaging::make_route("control", "submit_second"), [&](const streamrelay::messaging::Message& message) {
        ++second_delivered;
        assert(message.payload == streamrelay::core::ByteBuffer({2, 2, 2}));
        return streamrelay::core::success();
    });
    assert(second_subscribed.ok());

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

    DWORD timeout_ms = 500;
    auto first_client = connect_loopback(status.port, timeout_ms);
    send_all(first_client, handshake_request("admin-token-1"));
    auto first_step = event_loop.run_once(system_now, steady_now);
    assert(first_step.ok());
    assert(first_step.value().active_connections == 1);
    auto first_response = receive_text(first_client);
    assert(first_response.find("101 Switching Protocols") != std::string::npos);

    auto second_client = connect_loopback(status.port, timeout_ms);
    send_all(second_client, handshake_request("admin-token-2"));
    auto second_step = event_loop.run_once(system_now, steady_now);
    assert(second_step.ok());
    assert(second_step.value().active_connections == 2);
    auto second_response = receive_text(second_client);
    assert(second_response.find("101 Switching Protocols") != std::string::npos);
    assert(first_step.value().gateway.edge.transport_id != second_step.value().gateway.edge.transport_id);
    assert(first_step.value().gateway.edge.connection != second_step.value().gateway.edge.connection);

    status = event_loop.status();
    assert(status.accepted_connections == 2);
    assert(status.active_connections == 2);
    assert(metrics.counter("gateway.admin_accepted") == 2);

    auto no_ready = event_loop.pump_ready_frames_once(system_now, steady_now);
    assert(no_ready.ok());
    assert(no_ready.value().attempted == 0);
    assert(no_ready.value().succeeded == 0);
    assert(no_ready.value().failed == 0);

    auto first_envelope = make_envelope(100, "submit_first", system_now, streamrelay::core::ByteBuffer{1, 1, 1});
    auto first_frame = make_masked_websocket_envelope(first_envelope);
    send_all(first_client, first_frame);
    auto ready_pumped_first = event_loop.pump_ready_frames_once(system_now, steady_now);
    assert(ready_pumped_first.ok());
    assert(ready_pumped_first.value().attempted == 1);
    assert(ready_pumped_first.value().succeeded == 1);
    assert(ready_pumped_first.value().failed == 0);
    assert(first_delivered == 1);
    assert(second_delivered == 0);

    auto second_envelope = make_envelope(200, "submit_second", system_now, streamrelay::core::ByteBuffer{2, 2, 2});
    auto second_frame = make_masked_websocket_envelope(second_envelope);
    send_all(second_client, second_frame);
    auto ready_pumped_second = event_loop.pump_ready_frames_once(system_now, steady_now);
    assert(ready_pumped_second.ok());
    assert(ready_pumped_second.value().attempted == 1);
    assert(ready_pumped_second.value().succeeded == 1);
    assert(ready_pumped_second.value().failed == 0);
    assert(first_delivered == 1);
    assert(second_delivered == 1);
    assert(metrics.counter("gateway.frame_processed") == 2);

    first_frame = make_masked_websocket_envelope(first_envelope);
    send_all(first_client, first_frame);
    second_frame = make_masked_websocket_envelope(second_envelope);
    send_all(second_client, second_frame);
    auto batch_pumped = event_loop.pump_active_frames_once(system_now, steady_now);
    assert(batch_pumped.ok());
    assert(batch_pumped.value().attempted == 2);
    assert(batch_pumped.value().succeeded == 2);
    assert(batch_pumped.value().failed == 0);
    assert(first_delivered == 2);
    assert(second_delivered == 2);
    assert(metrics.counter("gateway.frame_processed") == 4);

    auto broadcast_written = event_loop.send_websocket_binary_to_all_once(streamrelay::core::ByteBuffer{8, 8});
    assert(broadcast_written.ok());
    assert(broadcast_written.value().attempted == 2);
    assert(broadcast_written.value().succeeded == 2);
    assert(broadcast_written.value().failed == 0);
    auto first_broadcast = receive_binary_frame(first_client);
    auto second_broadcast = receive_binary_frame(second_client);
    streamrelay::protocol::WebSocketFrameLimits server_limits;
    server_limits.require_masked_client_frames = false;
    auto first_decoded_broadcast = streamrelay::protocol::decode_websocket_frame(first_broadcast, server_limits);
    auto second_decoded_broadcast = streamrelay::protocol::decode_websocket_frame(second_broadcast, server_limits);
    assert(first_decoded_broadcast.ok());
    assert(second_decoded_broadcast.ok());
    assert(first_decoded_broadcast.value().payload == streamrelay::core::ByteBuffer({8, 8}));
    assert(second_decoded_broadcast.value().payload == streamrelay::core::ByteBuffer({8, 8}));

    auto second_written = event_loop.send_websocket_binary_once(second_step.value().gateway.edge.transport_id, streamrelay::core::ByteBuffer{9, 2});
    assert(second_written.ok());
    auto second_encoded_reply = receive_binary_frame(second_client);
    auto second_decoded_reply = streamrelay::protocol::decode_websocket_frame(second_encoded_reply, server_limits);
    assert(second_decoded_reply.ok());
    assert(second_decoded_reply.value().payload == streamrelay::core::ByteBuffer({9, 2}));
    status = event_loop.status();
    assert(status.frames_processed == 4);
    assert(status.frames_sent == 3);
    assert(status.failed_frames == 0);
    assert(status.failed_writes == 0);

    auto first_closed = event_loop.close_connection_once(first_step.value().gateway.edge.transport_id);
    assert(first_closed.ok());
    status = event_loop.status();
    assert(status.active_connections == 1);
    assert(status.closed_connections == 1);

    auto second_closed = event_loop.close_connection_once(second_step.value().gateway.edge.transport_id);
    assert(second_closed.ok());
    status = event_loop.status();
    assert(status.active_connections == 0);
    assert(status.closed_connections == 2);

    closesocket(first_client);
    closesocket(second_client);
    auto stopped = event_loop.stop();
    assert(stopped.ok());
    status = event_loop.status();
    assert(!status.running);
#else
    assert(!started.ok());
#endif

    return 0;
}
