#pragma once

#include <cstddef>
#include <cstdint>

#include "core/result.h"
#include "core/types.h"

namespace streamrelay::protocol {

enum class WebSocketOpcode : std::uint8_t {
    Continuation = 0x0,
    Text = 0x1,
    Binary = 0x2,
    Close = 0x8,
    Ping = 0x9,
    Pong = 0xA,
};

struct WebSocketFrame {
    bool fin{true};
    WebSocketOpcode opcode{WebSocketOpcode::Binary};
    bool masked{false};
    std::uint32_t masking_key{0};
    core::ByteBuffer payload;
};

struct WebSocketFrameLimits {
    std::size_t max_payload_bytes{1024 * 1024};
    bool require_masked_client_frames{true};
};

core::Result<core::ByteBuffer> encode_websocket_frame(const WebSocketFrame& frame);
core::Result<WebSocketFrame> decode_websocket_frame(const core::ByteBuffer& bytes, const WebSocketFrameLimits& limits);

} 
