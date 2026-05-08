#include "protocol/websocket_frame.h"

#include <limits>

namespace streamrelay::protocol {
namespace {

void append_u16_be(core::ByteBuffer& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void append_u64_be(core::ByteBuffer& out, std::uint64_t value) {
    for (int i = 7; i >= 0; --i) {
        out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xffU));
    }
}

std::uint16_t read_u16_be(const core::ByteBuffer& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U) | bytes[offset + 1]);
}

std::uint64_t read_u64_be(const core::ByteBuffer& bytes, std::size_t offset) {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8U) | bytes[offset + i];
    }
    return value;
}

bool is_known_opcode(std::uint8_t opcode) {
    return opcode == 0x0 || opcode == 0x1 || opcode == 0x2 || opcode == 0x8 || opcode == 0x9 || opcode == 0xA;
}

bool is_control_opcode(WebSocketOpcode opcode) {
    return opcode == WebSocketOpcode::Close || opcode == WebSocketOpcode::Ping || opcode == WebSocketOpcode::Pong;
}

std::uint32_t read_masking_key(const core::ByteBuffer& bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

void append_masking_key(core::ByteBuffer& out, std::uint32_t key) {
    out.push_back(static_cast<std::uint8_t>((key >> 24U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>((key >> 16U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>((key >> 8U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>(key & 0xffU));
}

std::uint8_t mask_byte(std::uint32_t key, std::size_t index) {
    const auto shift = static_cast<unsigned>((3U - (index % 4U)) * 8U);
    return static_cast<std::uint8_t>((key >> shift) & 0xffU);
}

} 

core::Result<core::ByteBuffer> encode_websocket_frame(const WebSocketFrame& frame) {
    if (frame.payload.size() > static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max())) {
        return core::make_error(core::ErrorCode::ResourceExhausted, "websocket payload is too large");
    }

    if (is_control_opcode(frame.opcode) && (!frame.fin || frame.payload.size() > 125)) {
        return core::make_error(core::ErrorCode::ProtocolError, "invalid websocket control frame");
    }

    core::ByteBuffer out;
    out.push_back(static_cast<std::uint8_t>((frame.fin ? 0x80U : 0x00U) | static_cast<std::uint8_t>(frame.opcode)));

    const auto payload_len = frame.payload.size();
    const auto mask_bit = frame.masked ? 0x80U : 0x00U;
    if (payload_len <= 125) {
        out.push_back(static_cast<std::uint8_t>(mask_bit | payload_len));
    } else if (payload_len <= 0xffffU) {
        out.push_back(static_cast<std::uint8_t>(mask_bit | 126U));
        append_u16_be(out, static_cast<std::uint16_t>(payload_len));
    } else {
        out.push_back(static_cast<std::uint8_t>(mask_bit | 127U));
        append_u64_be(out, static_cast<std::uint64_t>(payload_len));
    }

    if (frame.masked) {
        append_masking_key(out, frame.masking_key);
    }

    for (std::size_t i = 0; i < frame.payload.size(); ++i) {
        auto byte = frame.payload[i];
        if (frame.masked) {
            byte ^= mask_byte(frame.masking_key, i);
        }
        out.push_back(byte);
    }

    return out;
}

core::Result<WebSocketFrame> decode_websocket_frame(const core::ByteBuffer& bytes, const WebSocketFrameLimits& limits) {
    if (bytes.size() < 2) {
        return core::make_error(core::ErrorCode::ProtocolError, "websocket frame header is truncated");
    }

    const auto first = bytes[0];
    const auto rsv = first & 0x70U;
    if (rsv != 0) {
        return core::make_error(core::ErrorCode::ProtocolError, "websocket RSV bits are not supported");
    }

    const auto raw_opcode = static_cast<std::uint8_t>(first & 0x0fU);
    if (!is_known_opcode(raw_opcode)) {
        return core::make_error(core::ErrorCode::ProtocolError, "unknown websocket opcode");
    }

    WebSocketFrame frame;
    frame.fin = (first & 0x80U) != 0;
    frame.opcode = static_cast<WebSocketOpcode>(raw_opcode);

    const auto second = bytes[1];
    frame.masked = (second & 0x80U) != 0;
    if (limits.require_masked_client_frames && !frame.masked) {
        return core::make_error(core::ErrorCode::ProtocolError, "client websocket frame is not masked");
    }

    std::size_t offset = 2;
    std::uint64_t payload_len = second & 0x7fU;
    if (payload_len == 126) {
        if (bytes.size() < offset + 2) {
            return core::make_error(core::ErrorCode::ProtocolError, "websocket extended length is truncated");
        }
        payload_len = read_u16_be(bytes, offset);
        offset += 2;
    } else if (payload_len == 127) {
        if (bytes.size() < offset + 8) {
            return core::make_error(core::ErrorCode::ProtocolError, "websocket extended length is truncated");
        }
        payload_len = read_u64_be(bytes, offset);
        offset += 8;
        if ((payload_len & (1ULL << 63U)) != 0) {
            return core::make_error(core::ErrorCode::ProtocolError, "websocket payload length is not canonical");
        }
    }

    if (payload_len > limits.max_payload_bytes) {
        return core::make_error(core::ErrorCode::ResourceExhausted, "websocket payload exceeds configured limit");
    }

    if (frame.masked) {
        if (bytes.size() < offset + 4) {
            return core::make_error(core::ErrorCode::ProtocolError, "websocket masking key is truncated");
        }
        frame.masking_key = read_masking_key(bytes, offset);
        offset += 4;
    }

    if (bytes.size() != offset + static_cast<std::size_t>(payload_len)) {
        return core::make_error(core::ErrorCode::ProtocolError, "websocket frame size does not match payload length");
    }

    if (is_control_opcode(frame.opcode) && (!frame.fin || payload_len > 125)) {
        return core::make_error(core::ErrorCode::ProtocolError, "invalid websocket control frame");
    }

    frame.payload.reserve(static_cast<std::size_t>(payload_len));
    for (std::size_t i = 0; i < static_cast<std::size_t>(payload_len); ++i) {
        auto byte = bytes[offset + i];
        if (frame.masked) {
            byte ^= mask_byte(frame.masking_key, i);
        }
        frame.payload.push_back(byte);
    }

    return frame;
}

} 
