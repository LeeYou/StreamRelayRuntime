#pragma once

#include <chrono>
#include <string>

#include "core/result.h"
#include "gateway/gateway_edge.h"
#include "transport/websocket_handshake.h"

namespace streamrelay::gateway {

struct WebSocketGatewayAcceptResult {
    GatewayEdgeAcceptResult edge;
    std::string handshake_response;
};

class WebSocketGatewayAdapter {
public:
    explicit WebSocketGatewayAdapter(GatewayEdge& edge);

    core::Result<WebSocketGatewayAcceptResult> accept_admin_handshake(const std::string& raw_request, std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now);
    core::Result<WebSocketGatewayAcceptResult> accept_device_handshake(const std::string& raw_request, std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now);

private:
    static core::Result<std::string> required_header(const transport::WebSocketHandshakeRequest& request, const std::string& name);

    GatewayEdge& edge_;
};

} 
