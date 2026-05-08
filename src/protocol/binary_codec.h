#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "core/result.h"
#include "core/types.h"
#include "protocol/envelope.h"

namespace streamrelay::protocol {

struct FrameLimits {
    std::size_t max_envelope_bytes{64 * 1024};
    std::size_t max_payload_bytes{1024 * 1024};
};

struct FrameHeader {
    std::uint16_t magic{0x5352};
    std::uint16_t version{1};
    std::uint32_t flags{0};
    std::uint32_t header_len{24};
    std::uint32_t envelope_len{0};
    std::uint32_t payload_len{0};
    std::uint32_t header_crc{0};
};

core::Result<core::ByteBuffer> encode_envelope_frame(const Envelope& envelope);
core::Result<Envelope> decode_envelope_frame(const core::ByteBuffer& frame, const FrameLimits& limits, std::int64_t now_unix_ms);

std::int64_t to_unix_ms(std::chrono::system_clock::time_point time_point);

} 
