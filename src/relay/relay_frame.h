#pragma once

#include <cstddef>
#include <cstdint>

#include "core/result.h"
#include "core/types.h"

namespace streamrelay::relay {

using ChannelHandle = std::uint64_t;

enum RelayFrameFlags : std::uint16_t {
    RelayFrameData = 1U << 0U,
    RelayFrameAck = 1U << 1U,
    RelayFrameFin = 1U << 2U,
    RelayFrameReset = 1U << 3U,
    RelayFrameWindowUpdate = 1U << 4U,
};

struct RelayFrameLimits {
    std::size_t max_payload_bytes{1024 * 1024};
};

struct RelayFrameHeader {
    std::uint16_t magic{0x5246};
    std::uint16_t version{1};
    std::uint16_t flags{RelayFrameData};
    std::uint16_t header_len{40};
    ChannelHandle channel_handle{0};
    std::uint64_t sequence{0};
    std::uint64_t ack_sequence{0};
    std::uint32_t window_credit{0};
    std::uint32_t payload_len{0};
};

struct RelayFrame {
    RelayFrameHeader header;
    core::ByteBuffer payload;
};

core::Result<core::ByteBuffer> encode_relay_frame(const RelayFrame& frame);
core::Result<RelayFrame> decode_relay_frame(const core::ByteBuffer& bytes, const RelayFrameLimits& limits);

} 
