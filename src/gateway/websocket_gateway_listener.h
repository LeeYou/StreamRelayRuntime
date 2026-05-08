#pragma once

#include <chrono>
#include <cstddef>
#include <string>

#include "core/result.h"
#include "gateway/websocket_gateway_adapter.h"
#include "transport/tcp_listener.h"

namespace streamrelay::gateway {

struct WebSocketGatewayListenerOptions {
    std::size_t max_handshake_bytes{8192};
};

struct WebSocketGatewayListenerAcceptResult {
    WebSocketGatewayAcceptResult gateway;
    transport::TcpConnection connection;
};

class WebSocketGatewayListener {
public:
    WebSocketGatewayListener(transport::TcpListener& listener, WebSocketGatewayAdapter& adapter, WebSocketGatewayListenerOptions options = {});

    core::Result<WebSocketGatewayListenerAcceptResult> accept_admin_once(std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now);
    core::Result<WebSocketGatewayListenerAcceptResult> accept_device_once(std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now);

private:
    core::Result<std::string> read_handshake_request(transport::TcpConnection& connection) const;
    static core::ByteBuffer to_bytes(const std::string& value);

    transport::TcpListener& listener_;
    WebSocketGatewayAdapter& adapter_;
    WebSocketGatewayListenerOptions options_;
};

} 
