#pragma once

#include <string>
#include <unordered_map>

#include "core/result.h"

namespace streamrelay::transport {

struct WebSocketHandshakeRequest {
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
};

struct WebSocketHandshakeResponse {
    std::string accept_key;
    std::string raw_response;
};

class WebSocketHandshakeCodec {
public:
    static core::Result<WebSocketHandshakeRequest> parse_request(const std::string& raw_request);
    static core::Result<WebSocketHandshakeResponse> build_response(const WebSocketHandshakeRequest& request);
    static std::string header_value(const WebSocketHandshakeRequest& request, const std::string& name);

private:
    static std::string trim(const std::string& value);
    static std::string lower(const std::string& value);
    static std::string websocket_accept_key(const std::string& client_key);
};

} 
