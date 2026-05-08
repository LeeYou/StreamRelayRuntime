#include "relay/relay_frame.h"

#include <limits>

namespace streamrelay::relay {
namespace {

constexpr std::uint16_t kMagic = 0x5246;
constexpr std::uint16_t kVersion = 1;
constexpr std::uint16_t kHeaderLen = 40;

void append_u16(core::ByteBuffer& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void append_u32(core::ByteBuffer& out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xffU));
    }
}

void append_u64(core::ByteBuffer& out, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xffU));
    }
}

std::uint16_t read_u16(const core::ByteBuffer& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset] | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8U));
}

std::uint32_t read_u32(const core::ByteBuffer& bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(bytes[offset + i]) << (i * 8);
    }
    return value;
}

std::uint64_t read_u64(const core::ByteBuffer& bytes, std::size_t offset) {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(bytes[offset + i]) << (i * 8);
    }
    return value;
}

} 

core::Result<core::ByteBuffer> encode_relay_frame(const RelayFrame& frame) {
    if (frame.header.channel_handle == 0) {
        return core::make_error(core::ErrorCode::InvalidArgument, "channel_handle is required");
    }

    if (frame.payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        return core::make_error(core::ErrorCode::ResourceExhausted, "relay payload is too large");
    }

    core::ByteBuffer out;
    out.reserve(kHeaderLen + frame.payload.size());
    append_u16(out, kMagic);
    append_u16(out, kVersion);
    append_u16(out, frame.header.flags);
    append_u16(out, kHeaderLen);
    append_u64(out, frame.header.channel_handle);
    append_u64(out, frame.header.sequence);
    append_u64(out, frame.header.ack_sequence);
    append_u32(out, frame.header.window_credit);
    append_u32(out, static_cast<std::uint32_t>(frame.payload.size()));
    out.insert(out.end(), frame.payload.begin(), frame.payload.end());
    return out;
}

core::Result<RelayFrame> decode_relay_frame(const core::ByteBuffer& bytes, const RelayFrameLimits& limits) {
    if (bytes.size() < kHeaderLen) {
        return core::make_error(core::ErrorCode::ProtocolError, "relay frame header is truncated");
    }

    RelayFrame frame;
    frame.header.magic = read_u16(bytes, 0);
    frame.header.version = read_u16(bytes, 2);
    frame.header.flags = read_u16(bytes, 4);
    frame.header.header_len = read_u16(bytes, 6);
    frame.header.channel_handle = read_u64(bytes, 8);
    frame.header.sequence = read_u64(bytes, 16);
    frame.header.ack_sequence = read_u64(bytes, 24);
    frame.header.window_credit = read_u32(bytes, 32);
    frame.header.payload_len = read_u32(bytes, 36);

    if (frame.header.magic != kMagic) {
        return core::make_error(core::ErrorCode::ProtocolError, "invalid relay frame magic");
    }

    if (frame.header.version != kVersion) {
        return core::make_error(core::ErrorCode::ProtocolError, "unsupported relay frame version");
    }

    if (frame.header.header_len != kHeaderLen) {
        return core::make_error(core::ErrorCode::ProtocolError, "invalid relay frame header length");
    }

    if (frame.header.channel_handle == 0) {
        return core::make_error(core::ErrorCode::ProtocolError, "relay channel_handle is missing");
    }

    if (frame.header.payload_len > limits.max_payload_bytes) {
        return core::make_error(core::ErrorCode::ResourceExhausted, "relay payload exceeds configured limit");
    }

    const auto expected_size = static_cast<std::size_t>(kHeaderLen) + frame.header.payload_len;
    if (bytes.size() != expected_size) {
        return core::make_error(core::ErrorCode::ProtocolError, "relay frame size does not match payload length");
    }

    frame.payload.assign(bytes.begin() + kHeaderLen, bytes.end());
    return frame;
}

} 
