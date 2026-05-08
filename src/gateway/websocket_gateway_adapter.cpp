#include "gateway/websocket_gateway_adapter.h"

namespace streamrelay::gateway {

WebSocketGatewayAdapter::WebSocketGatewayAdapter(GatewayEdge& edge) : edge_(edge) {}

core::Result<WebSocketGatewayAcceptResult> WebSocketGatewayAdapter::accept_admin_handshake(const std::string& raw_request, std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now) {
    auto parsed = transport::WebSocketHandshakeCodec::parse_request(raw_request);
    if (!parsed.ok()) {
        return parsed.error();
    }
    auto handshake = transport::WebSocketHandshakeCodec::build_response(parsed.value());
    if (!handshake.ok()) {
        return handshake.error();
    }
    auto token = required_header(parsed.value(), "x-streamrelay-token");
    if (!token.ok()) {
        return token.error();
    }
    auto accepted = edge_.accept_admin(token.value(), system_now, steady_now);
    if (!accepted.ok()) {
        return accepted.error();
    }
    return WebSocketGatewayAcceptResult{accepted.value(), handshake.value().raw_response};
}

core::Result<WebSocketGatewayAcceptResult> WebSocketGatewayAdapter::accept_device_handshake(const std::string& raw_request, std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now) {
    auto parsed = transport::WebSocketHandshakeCodec::parse_request(raw_request);
    if (!parsed.ok()) {
        return parsed.error();
    }
    auto handshake = transport::WebSocketHandshakeCodec::build_response(parsed.value());
    if (!handshake.ok()) {
        return handshake.error();
    }
    auto token = required_header(parsed.value(), "x-streamrelay-token");
    if (!token.ok()) {
        return token.error();
    }
    auto device_id = required_header(parsed.value(), "x-streamrelay-device-id");
    if (!device_id.ok()) {
        return device_id.error();
    }
    auto accepted = edge_.accept_device(token.value(), device_id.value(), system_now, steady_now);
    if (!accepted.ok()) {
        return accepted.error();
    }
    return WebSocketGatewayAcceptResult{accepted.value(), handshake.value().raw_response};
}

core::Result<std::string> WebSocketGatewayAdapter::required_header(const transport::WebSocketHandshakeRequest& request, const std::string& name) {
    auto value = transport::WebSocketHandshakeCodec::header_value(request, name);
    if (value.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "required websocket gateway header is missing");
    }
    return value;
}

} 
