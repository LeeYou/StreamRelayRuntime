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

streamrelay::core::ByteBuffer make_unmasked_websocket_envelope(const streamrelay::protocol::Envelope& envelope) {
    auto encoded_envelope = streamrelay::protocol::encode_envelope_frame(envelope);
    assert(encoded_envelope.ok());
    streamrelay::protocol::WebSocketFrame websocket;
    websocket.masked = false;
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
    int device_delivered = 0;
    auto device_subscribed = bus.subscribe(streamrelay::messaging::make_route("device", "telemetry"), [&](const streamrelay::messaging::Message& message) {
        ++device_delivered;
        assert(message.service == "device");
        assert(message.method == "telemetry");
        return streamrelay::core::success();
    });
    assert(device_subscribed.ok());
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

    const auto request = handshake_request("admin-token");
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

    auto written = event_loop.send_websocket_binary_once(step.value().gateway.edge.transport_id, streamrelay::core::ByteBuffer{9, 8, 7});
    assert(written.ok());
    char server_frame[512]{};
    auto received_server_frame = recv(client, server_frame, sizeof(server_frame), 0);
    assert(received_server_frame > 0);
    streamrelay::core::ByteBuffer encoded_server_frame(server_frame, server_frame + received_server_frame);
    streamrelay::protocol::WebSocketFrameLimits server_frame_limits;
    server_frame_limits.require_masked_client_frames = false;
    auto decoded_server_frame = streamrelay::protocol::decode_websocket_frame(encoded_server_frame, server_frame_limits);
    assert(decoded_server_frame.ok());
    assert(decoded_server_frame.value().payload == streamrelay::core::ByteBuffer({9, 8, 7}));
    status = event_loop.status();
    assert(status.frames_sent == 1);
    assert(status.failed_writes == 0);

    closesocket(client);
    auto stopped = event_loop.stop();
    assert(stopped.ok());
    status = event_loop.status();
    assert(!status.running);
    assert(status.active_connections == 0);

    options.accept_kind = streamrelay::gateway::GatewaySocketAcceptKind::Device;
    auto device_started = event_loop.start(options);
    assert(device_started.ok());
    status = event_loop.status();
    assert(status.running);
    assert(status.port > 0);

    auto device_client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(device_client != INVALID_SOCKET);
    receive_timeout_result = setsockopt(device_client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    send_timeout_result = setsockopt(device_client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    assert(receive_timeout_result == 0);
    assert(send_timeout_result == 0);

    address.sin_port = htons(status.port);
    connect_result = connect(device_client, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    assert(connect_result != SOCKET_ERROR);

    const auto device_request = handshake_request("admin-token", "device-1");
    sent_request = send(device_client, device_request.data(), static_cast<int>(device_request.size()), 0);
    assert(sent_request == static_cast<int>(device_request.size()));

    auto device_step = event_loop.run_once(system_now, steady_now);
    assert(device_step.ok());
    assert(device_step.value().active_connections == 1);
    assert(device_step.value().gateway.edge.session_id.find("tenant-1:device-1:") == 0);
    assert(metrics.counter("gateway.device_accepted") == 1);

    received = recv(device_client, response, sizeof(response), 0);
    assert(received > 0);
    const std::string raw_device_response{response, static_cast<std::size_t>(received)};
    assert(raw_device_response.find("101 Switching Protocols") != std::string::npos);

    auto found_device = devices.find_device("tenant-1", "device-1", system_now);
    assert(found_device.ok());
    assert(found_device.value().presence.connection.has_value());
    assert(found_device.value().presence.session_id == device_step.value().gateway.edge.session_id);

    streamrelay::protocol::Envelope device_envelope;
    device_envelope.request_id = 101;
    device_envelope.trace_id = 201;
    device_envelope.service = "device";
    device_envelope.method = "telemetry";
    device_envelope.deadline_unix_ms = streamrelay::protocol::to_unix_ms(system_now + std::chrono::seconds{10});
    device_envelope.payload = streamrelay::core::ByteBuffer{4, 5, 6};
    websocket_frame = make_masked_websocket_envelope(device_envelope);
    sent_frame = send(device_client, reinterpret_cast<const char*>(websocket_frame.data()), static_cast<int>(websocket_frame.size()), 0);
    assert(sent_frame == static_cast<int>(websocket_frame.size()));

    pumped = event_loop.pump_frame_once(device_step.value().gateway.edge.transport_id, system_now, steady_now);
    assert(pumped.ok());
    assert(device_delivered == 1);
    assert(metrics.counter("gateway.frame_processed") == 2);
    status = event_loop.status();
    assert(status.frames_processed == 1);
    assert(status.failed_frames == 0);

    auto device_client_reconnect = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(device_client_reconnect != INVALID_SOCKET);
    receive_timeout_result = setsockopt(device_client_reconnect, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    send_timeout_result = setsockopt(device_client_reconnect, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    assert(receive_timeout_result == 0);
    assert(send_timeout_result == 0);

    connect_result = connect(device_client_reconnect, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    assert(connect_result != SOCKET_ERROR);
    sent_request = send(device_client_reconnect, device_request.data(), static_cast<int>(device_request.size()), 0);
    assert(sent_request == static_cast<int>(device_request.size()));

    auto reconnected_device_step = event_loop.run_once(system_now, steady_now);
    assert(reconnected_device_step.ok());
    assert(reconnected_device_step.value().active_connections == 2);
    assert(reconnected_device_step.value().gateway.edge.session_id.find("tenant-1:device-1:") == 0);
    assert(metrics.counter("gateway.device_accepted") == 2);

    received = recv(device_client_reconnect, response, sizeof(response), 0);
    assert(received > 0);
    const std::string raw_reconnected_device_response{response, static_cast<std::size_t>(received)};
    assert(raw_reconnected_device_response.find("101 Switching Protocols") != std::string::npos);

    found_device = devices.find_device("tenant-1", "device-1", system_now);
    assert(found_device.ok());
    assert(found_device.value().presence.connection.has_value());
    assert(found_device.value().presence.connection.value() == reconnected_device_step.value().gateway.edge.connection);
    assert(found_device.value().presence.session_id == reconnected_device_step.value().gateway.edge.session_id);

    auto stale_offline = devices.mark_offline("tenant-1", "device-1", device_step.value().gateway.edge.connection);
    assert(!stale_offline.ok());
    assert(stale_offline.error().code == streamrelay::core::ErrorCode::InvalidState);
    found_device = devices.find_device("tenant-1", "device-1", system_now);
    assert(found_device.ok());
    assert(found_device.value().presence.connection.has_value());
    assert(found_device.value().presence.connection.value() == reconnected_device_step.value().gateway.edge.connection);

    auto stale_session_close = sessions.close_by_connection(device_step.value().gateway.edge.connection);
    assert(!stale_session_close.ok());
    assert(stale_session_close.error().code == streamrelay::core::ErrorCode::NotFound);
    auto active_session = sessions.find_by_connection(reconnected_device_step.value().gateway.edge.connection, system_now);
    assert(active_session.ok());
    assert(active_session.value().session_id == reconnected_device_step.value().gateway.edge.session_id);

    auto closed_old_device = event_loop.close_connection_once(device_step.value().gateway.edge.transport_id);
    assert(closed_old_device.ok());
    status = event_loop.status();
    assert(status.active_connections == 1);
    assert(status.closed_connections == 1);
    assert(status.failed_closes == 0);

    auto unmasked_frame = make_unmasked_websocket_envelope(device_envelope);
    sent_frame = send(device_client_reconnect, reinterpret_cast<const char*>(unmasked_frame.data()), static_cast<int>(unmasked_frame.size()), 0);
    assert(sent_frame == static_cast<int>(unmasked_frame.size()));
    auto rejected_unmasked = event_loop.pump_frame_once(reconnected_device_step.value().gateway.edge.transport_id, system_now, steady_now);
    assert(!rejected_unmasked.ok());
    assert(rejected_unmasked.error().code == streamrelay::core::ErrorCode::ProtocolError);
    assert(metrics.counter("gateway.frame_rejected") == 1);
    status = event_loop.status();
    assert(status.failed_frames == 1);

    streamrelay::protocol::Envelope expired_envelope;
    expired_envelope.request_id = 102;
    expired_envelope.trace_id = 202;
    expired_envelope.service = "device";
    expired_envelope.method = "telemetry";
    expired_envelope.deadline_unix_ms = streamrelay::protocol::to_unix_ms(system_now - std::chrono::seconds{1});
    expired_envelope.payload = streamrelay::core::ByteBuffer{7};
    websocket_frame = make_masked_websocket_envelope(expired_envelope);
    sent_frame = send(device_client_reconnect, reinterpret_cast<const char*>(websocket_frame.data()), static_cast<int>(websocket_frame.size()), 0);
    assert(sent_frame == static_cast<int>(websocket_frame.size()));
    auto rejected_expired = event_loop.pump_frame_once(reconnected_device_step.value().gateway.edge.transport_id, system_now, steady_now);
    assert(!rejected_expired.ok());
    assert(rejected_expired.error().code == streamrelay::core::ErrorCode::DeadlineExceeded);
    assert(metrics.counter("gateway.frame_rejected") == 2);
    status = event_loop.status();
    assert(status.failed_frames == 2);

    closesocket(device_client);
    closesocket(device_client_reconnect);
    stopped = event_loop.stop();
    assert(stopped.ok());
    status = event_loop.status();
    assert(!status.running);
    assert(status.active_connections == 0);

    options.accept_kind = streamrelay::gateway::GatewaySocketAcceptKind::Admin;
    options.max_frame_bytes = 4;
    auto limited_started = event_loop.start(options);
    assert(limited_started.ok());
    status = event_loop.status();
    assert(status.running);
    assert(status.port > 0);

    auto limited_client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(limited_client != INVALID_SOCKET);
    receive_timeout_result = setsockopt(limited_client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    send_timeout_result = setsockopt(limited_client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    assert(receive_timeout_result == 0);
    assert(send_timeout_result == 0);

    address.sin_port = htons(status.port);
    connect_result = connect(limited_client, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    assert(connect_result != SOCKET_ERROR);
    sent_request = send(limited_client, request.data(), static_cast<int>(request.size()), 0);
    assert(sent_request == static_cast<int>(request.size()));

    auto limited_step = event_loop.run_once(system_now, steady_now);
    assert(limited_step.ok());
    received = recv(limited_client, response, sizeof(response), 0);
    assert(received > 0);

    streamrelay::protocol::WebSocketFrame oversized_frame;
    oversized_frame.masked = true;
    oversized_frame.masking_key = 0x01020304;
    oversized_frame.payload = streamrelay::core::ByteBuffer{1, 2, 3, 4, 5, 6, 7, 8};
    auto encoded_oversized = streamrelay::protocol::encode_websocket_frame(oversized_frame);
    assert(encoded_oversized.ok());
    sent_frame = send(limited_client, reinterpret_cast<const char*>(encoded_oversized.value().data()), static_cast<int>(encoded_oversized.value().size()), 0);
    assert(sent_frame == static_cast<int>(encoded_oversized.value().size()));
    auto rejected_oversized = event_loop.pump_frame_once(limited_step.value().gateway.edge.transport_id, system_now, steady_now);
    assert(!rejected_oversized.ok());
    assert(rejected_oversized.error().code == streamrelay::core::ErrorCode::ResourceExhausted);
    status = event_loop.status();
    assert(status.failed_frames == 1);

    closesocket(limited_client);
    stopped = event_loop.stop();
    assert(stopped.ok());
    status = event_loop.status();
    assert(!status.running);
#else
    assert(!started.ok());
#endif

    return 0;
}
