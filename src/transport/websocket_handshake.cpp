#include "transport/websocket_handshake.h"

#include <array>
#include <cstdint>
#include <sstream>
#include <vector>

namespace streamrelay::transport {
namespace {

std::uint32_t rotate_left(std::uint32_t value, std::uint32_t bits) {
    return (value << bits) | (value >> (32U - bits));
}

void append_be(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

std::array<std::uint8_t, 20> sha1(const std::string& input) {
    std::vector<std::uint8_t> data(input.begin(), input.end());
    const auto bit_length = static_cast<std::uint64_t>(data.size()) * 8ULL;
    data.push_back(0x80U);
    while ((data.size() % 64U) != 56U) {
        data.push_back(0U);
    }
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<std::uint8_t>((bit_length >> (i * 8)) & 0xffU));
    }

    std::uint32_t h0 = 0x67452301U;
    std::uint32_t h1 = 0xefcdab89U;
    std::uint32_t h2 = 0x98badcfeU;
    std::uint32_t h3 = 0x10325476U;
    std::uint32_t h4 = 0xc3d2e1f0U;

    for (std::size_t chunk = 0; chunk < data.size(); chunk += 64U) {
        std::array<std::uint32_t, 80> w{};
        for (std::size_t i = 0; i < 16U; ++i) {
            const auto offset = chunk + i * 4U;
            w[i] = (static_cast<std::uint32_t>(data[offset]) << 24U) |
                   (static_cast<std::uint32_t>(data[offset + 1U]) << 16U) |
                   (static_cast<std::uint32_t>(data[offset + 2U]) << 8U) |
                   static_cast<std::uint32_t>(data[offset + 3U]);
        }
        for (std::size_t i = 16U; i < 80U; ++i) {
            w[i] = rotate_left(w[i - 3U] ^ w[i - 8U] ^ w[i - 14U] ^ w[i - 16U], 1U);
        }

        auto a = h0;
        auto b = h1;
        auto c = h2;
        auto d = h3;
        auto e = h4;

        for (std::size_t i = 0; i < 80U; ++i) {
            std::uint32_t f = 0;
            std::uint32_t k = 0;
            if (i < 20U) {
                f = (b & c) | ((~b) & d);
                k = 0x5a827999U;
            } else if (i < 40U) {
                f = b ^ c ^ d;
                k = 0x6ed9eba1U;
            } else if (i < 60U) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8f1bbcdcU;
            } else {
                f = b ^ c ^ d;
                k = 0xca62c1d6U;
            }
            const auto temp = rotate_left(a, 5U) + f + e + k + w[i];
            e = d;
            d = c;
            c = rotate_left(b, 30U);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    std::vector<std::uint8_t> digest;
    append_be(digest, h0);
    append_be(digest, h1);
    append_be(digest, h2);
    append_be(digest, h3);
    append_be(digest, h4);

    std::array<std::uint8_t, 20> out{};
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = digest[i];
    }
    return out;
}

std::string base64_encode(const std::array<std::uint8_t, 20>& bytes) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < bytes.size(); i += 3U) {
        const auto remaining = bytes.size() - i;
        const auto b0 = bytes[i];
        const auto b1 = remaining > 1U ? bytes[i + 1U] : 0U;
        const auto b2 = remaining > 2U ? bytes[i + 2U] : 0U;
        const auto triple = (static_cast<std::uint32_t>(b0) << 16U) | (static_cast<std::uint32_t>(b1) << 8U) | static_cast<std::uint32_t>(b2);
        out.push_back(alphabet[(triple >> 18U) & 0x3fU]);
        out.push_back(alphabet[(triple >> 12U) & 0x3fU]);
        out.push_back(remaining > 1U ? alphabet[(triple >> 6U) & 0x3fU] : '=');
        out.push_back(remaining > 2U ? alphabet[triple & 0x3fU] : '=');
    }
    return out;
}

} 

core::Result<WebSocketHandshakeRequest> WebSocketHandshakeCodec::parse_request(const std::string& raw_request) {
    std::istringstream stream(raw_request);
    std::string line;
    if (!std::getline(stream, line)) {
        return core::make_error(core::ErrorCode::ProtocolError, "websocket handshake request is empty");
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    std::istringstream request_line(line);
    WebSocketHandshakeRequest request;
    std::string version;
    request_line >> request.method >> request.path >> version;
    if (request.method != "GET" || request.path.empty() || version != "HTTP/1.1") {
        return core::make_error(core::ErrorCode::ProtocolError, "invalid websocket request line");
    }

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            break;
        }
        const auto separator = line.find(':');
        if (separator == std::string::npos) {
            return core::make_error(core::ErrorCode::ProtocolError, "invalid websocket header line");
        }
        request.headers[lower(trim(line.substr(0, separator)))] = trim(line.substr(separator + 1U));
    }

    return request;
}

core::Result<WebSocketHandshakeResponse> WebSocketHandshakeCodec::build_response(const WebSocketHandshakeRequest& request) {
    if (lower(header_value(request, "upgrade")) != "websocket") {
        return core::make_error(core::ErrorCode::ProtocolError, "websocket upgrade header is required");
    }
    const auto connection = lower(header_value(request, "connection"));
    if (connection.find("upgrade") == std::string::npos) {
        return core::make_error(core::ErrorCode::ProtocolError, "websocket connection upgrade token is required");
    }
    if (header_value(request, "sec-websocket-version") != "13") {
        return core::make_error(core::ErrorCode::ProtocolError, "unsupported websocket version");
    }
    const auto client_key = header_value(request, "sec-websocket-key");
    if (client_key.empty()) {
        return core::make_error(core::ErrorCode::ProtocolError, "websocket key is required");
    }

    WebSocketHandshakeResponse response;
    response.accept_key = websocket_accept_key(client_key);
    response.raw_response = "HTTP/1.1 101 Switching Protocols\r\n";
    response.raw_response += "Upgrade: websocket\r\n";
    response.raw_response += "Connection: Upgrade\r\n";
    response.raw_response += "Sec-WebSocket-Accept: " + response.accept_key + "\r\n\r\n";
    return response;
}

std::string WebSocketHandshakeCodec::header_value(const WebSocketHandshakeRequest& request, const std::string& name) {
    const auto it = request.headers.find(lower(name));
    return it == request.headers.end() ? std::string{} : it->second;
}

std::string WebSocketHandshakeCodec::trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t");
    return value.substr(begin, end - begin + 1U);
}

std::string WebSocketHandshakeCodec::lower(const std::string& value) {
    std::string out = value;
    for (auto& c : out) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

std::string WebSocketHandshakeCodec::websocket_accept_key(const std::string& client_key) {
    return base64_encode(sha1(client_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
}

} 
