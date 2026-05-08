#include "gateway/websocket_gateway_listener.h"

#include <algorithm>

namespace streamrelay::gateway {

WebSocketGatewayListener::WebSocketGatewayListener(transport::TcpListener& listener, WebSocketGatewayAdapter& adapter, WebSocketGatewayListenerOptions options)
    : listener_(listener), adapter_(adapter), options_(options) {}

core::Result<WebSocketGatewayListenerAcceptResult> WebSocketGatewayListener::accept_admin_once(std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now) {
    auto connection = listener_.accept_once();
    if (!connection.ok()) {
        return connection.error();
    }
    auto accepted_connection = std::move(connection).value();
    auto request = read_handshake_request(accepted_connection);
    if (!request.ok()) {
        return request.error();
    }
    auto gateway = adapter_.accept_admin_handshake(request.value(), system_now, steady_now);
    if (!gateway.ok()) {
        return gateway.error();
    }
    auto written = accepted_connection.write_all(to_bytes(gateway.value().handshake_response));
    if (!written.ok()) {
        return written.error();
    }
    return WebSocketGatewayListenerAcceptResult{gateway.value(), std::move(accepted_connection)};
}

core::Result<WebSocketGatewayListenerAcceptResult> WebSocketGatewayListener::accept_device_once(std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now) {
    auto connection = listener_.accept_once();
    if (!connection.ok()) {
        return connection.error();
    }
    auto accepted_connection = std::move(connection).value();
    auto request = read_handshake_request(accepted_connection);
    if (!request.ok()) {
        return request.error();
    }
    auto gateway = adapter_.accept_device_handshake(request.value(), system_now, steady_now);
    if (!gateway.ok()) {
        return gateway.error();
    }
    auto written = accepted_connection.write_all(to_bytes(gateway.value().handshake_response));
    if (!written.ok()) {
        return written.error();
    }
    return WebSocketGatewayListenerAcceptResult{gateway.value(), std::move(accepted_connection)};
}

core::Result<std::string> WebSocketGatewayListener::read_handshake_request(transport::TcpConnection& connection) const {
    if (options_.max_handshake_bytes == 0) {
        return core::make_error(core::ErrorCode::InvalidArgument, "websocket gateway listener max_handshake_bytes must be positive");
    }

    std::string raw;
    while (raw.find("\r\n\r\n") == std::string::npos) {
        const auto remaining = options_.max_handshake_bytes - raw.size();
        if (remaining == 0) {
            return core::make_error(core::ErrorCode::ResourceExhausted, "websocket handshake request is too large");
        }
        auto chunk = connection.read_some(std::min<std::size_t>(remaining, 1024));
        if (!chunk.ok()) {
            return chunk.error();
        }
        raw.append(reinterpret_cast<const char*>(chunk.value().data()), chunk.value().size());
    }
    return raw;
}

core::ByteBuffer WebSocketGatewayListener::to_bytes(const std::string& value) {
    return core::ByteBuffer(value.begin(), value.end());
}

} 
