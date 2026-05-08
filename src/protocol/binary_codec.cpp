#include "protocol/binary_codec.h"

#include <cstring>
#include <limits>

namespace streamrelay::protocol {
namespace {

constexpr std::uint16_t kMagic = 0x5352;
constexpr std::uint16_t kVersion = 1;
constexpr std::uint32_t kHeaderLen = 24;
constexpr char kUnitSeparator = '\x1f';
constexpr char kRecordSeparator = '\x1e';

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

void append_i64(core::ByteBuffer& out, std::int64_t value) {
    append_u64(out, static_cast<std::uint64_t>(value));
}

std::uint16_t read_u16(const core::ByteBuffer& in, std::size_t offset) {
    return static_cast<std::uint16_t>(in[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(in[offset + 1]) << 8U);
}

std::uint32_t read_u32(const core::ByteBuffer& in, std::size_t offset) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(in[offset + i]) << (i * 8);
    }
    return value;
}

std::uint64_t read_u64(const core::ByteBuffer& in, std::size_t offset) {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(in[offset + i]) << (i * 8);
    }
    return value;
}

std::int64_t read_i64(const core::ByteBuffer& in, std::size_t offset) {
    return static_cast<std::int64_t>(read_u64(in, offset));
}

void append_string(core::ByteBuffer& out, const std::string& value) {
    append_u32(out, static_cast<std::uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

core::Result<std::string> read_string(const core::ByteBuffer& in, std::size_t& offset) {
    if (offset + 4 > in.size()) {
        return core::make_error(core::ErrorCode::ProtocolError, "string length is truncated");
    }

    const auto length = read_u32(in, offset);
    offset += 4;
    if (offset + length > in.size()) {
        return core::make_error(core::ErrorCode::ProtocolError, "string value is truncated");
    }

    std::string value{reinterpret_cast<const char*>(in.data() + offset), length};
    offset += length;
    return value;
}

core::ByteBuffer serialize_envelope_metadata(const Envelope& envelope) {
    core::ByteBuffer out;
    append_u32(out, envelope.version);
    append_u64(out, envelope.request_id);
    append_u64(out, envelope.trace_id);
    append_string(out, envelope.source);
    append_string(out, envelope.target);
    append_string(out, envelope.service);
    append_string(out, envelope.method);
    append_string(out, envelope.session_id);
    append_string(out, envelope.principal_id);
    append_i64(out, envelope.deadline_unix_ms);
    append_u32(out, static_cast<std::uint32_t>(envelope.labels.size()));
    for (const auto& label : envelope.labels) {
        append_string(out, label.first);
        append_string(out, label.second);
    }
    append_string(out, envelope.payload_type);
    return out;
}

core::Result<Envelope> deserialize_envelope_metadata(const core::ByteBuffer& metadata) {
    Envelope envelope;
    std::size_t offset = 0;

    if (metadata.size() < 4 + 8 + 8) {
        return core::make_error(core::ErrorCode::ProtocolError, "envelope metadata is truncated");
    }

    envelope.version = read_u32(metadata, offset);
    offset += 4;
    envelope.request_id = read_u64(metadata, offset);
    offset += 8;
    envelope.trace_id = read_u64(metadata, offset);
    offset += 8;

    auto source = read_string(metadata, offset);
    if (!source.ok()) {
        return source.error();
    }
    envelope.source = std::move(source).value();

    auto target = read_string(metadata, offset);
    if (!target.ok()) {
        return target.error();
    }
    envelope.target = std::move(target).value();

    auto service = read_string(metadata, offset);
    if (!service.ok()) {
        return service.error();
    }
    envelope.service = std::move(service).value();

    auto method = read_string(metadata, offset);
    if (!method.ok()) {
        return method.error();
    }
    envelope.method = std::move(method).value();

    auto session_id = read_string(metadata, offset);
    if (!session_id.ok()) {
        return session_id.error();
    }
    envelope.session_id = std::move(session_id).value();

    auto principal_id = read_string(metadata, offset);
    if (!principal_id.ok()) {
        return principal_id.error();
    }
    envelope.principal_id = std::move(principal_id).value();

    if (offset + 8 + 4 > metadata.size()) {
        return core::make_error(core::ErrorCode::ProtocolError, "envelope metadata is truncated");
    }

    envelope.deadline_unix_ms = read_i64(metadata, offset);
    offset += 8;

    const auto label_count = read_u32(metadata, offset);
    offset += 4;
    for (std::uint32_t i = 0; i < label_count; ++i) {
        auto key = read_string(metadata, offset);
        if (!key.ok()) {
            return key.error();
        }

        auto value = read_string(metadata, offset);
        if (!value.ok()) {
            return value.error();
        }

        envelope.labels.emplace(std::move(key).value(), std::move(value).value());
    }

    auto payload_type = read_string(metadata, offset);
    if (!payload_type.ok()) {
        return payload_type.error();
    }
    envelope.payload_type = std::move(payload_type).value();

    if (offset != metadata.size()) {
        return core::make_error(core::ErrorCode::ProtocolError, "envelope metadata has trailing bytes");
    }

    return envelope;
}

std::uint32_t checksum_header(const core::ByteBuffer& frame) {
    std::uint32_t checksum = 0;
    for (std::size_t i = 0; i < kHeaderLen - 4; ++i) {
        checksum = (checksum * 131U) + frame[i];
    }
    return checksum;
}

} 

core::Result<core::ByteBuffer> encode_envelope_frame(const Envelope& envelope) {
    if (envelope.service.empty() || envelope.method.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "service and method are required");
    }

    const auto metadata = serialize_envelope_metadata(envelope);
    if (metadata.size() > std::numeric_limits<std::uint32_t>::max() ||
        envelope.payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        return core::make_error(core::ErrorCode::ResourceExhausted, "frame is too large");
    }

    core::ByteBuffer frame;
    frame.reserve(kHeaderLen + metadata.size() + envelope.payload.size());
    append_u16(frame, kMagic);
    append_u16(frame, kVersion);
    append_u32(frame, 0);
    append_u32(frame, kHeaderLen);
    append_u32(frame, static_cast<std::uint32_t>(metadata.size()));
    append_u32(frame, static_cast<std::uint32_t>(envelope.payload.size()));
    append_u32(frame, 0);

    const auto checksum = checksum_header(frame);
    frame[20] = static_cast<std::uint8_t>(checksum & 0xffU);
    frame[21] = static_cast<std::uint8_t>((checksum >> 8U) & 0xffU);
    frame[22] = static_cast<std::uint8_t>((checksum >> 16U) & 0xffU);
    frame[23] = static_cast<std::uint8_t>((checksum >> 24U) & 0xffU);

    frame.insert(frame.end(), metadata.begin(), metadata.end());
    frame.insert(frame.end(), envelope.payload.begin(), envelope.payload.end());
    return frame;
}

core::Result<Envelope> decode_envelope_frame(const core::ByteBuffer& frame, const FrameLimits& limits, std::int64_t now_unix_ms) {
    if (frame.size() < kHeaderLen) {
        return core::make_error(core::ErrorCode::ProtocolError, "frame header is truncated");
    }

    if (read_u16(frame, 0) != kMagic) {
        return core::make_error(core::ErrorCode::ProtocolError, "invalid frame magic");
    }

    if (read_u16(frame, 2) != kVersion) {
        return core::make_error(core::ErrorCode::ProtocolError, "unsupported frame version");
    }

    const auto flags = read_u32(frame, 4);
    if ((flags & 0x80000000U) != 0) {
        return core::make_error(core::ErrorCode::ProtocolError, "unknown critical flag");
    }

    const auto header_len = read_u32(frame, 8);
    if (header_len != kHeaderLen) {
        return core::make_error(core::ErrorCode::ProtocolError, "invalid header length");
    }

    const auto envelope_len = read_u32(frame, 12);
    const auto payload_len = read_u32(frame, 16);
    const auto header_crc = read_u32(frame, 20);
    if (header_crc != checksum_header(frame)) {
        return core::make_error(core::ErrorCode::ProtocolError, "invalid header checksum");
    }

    if (envelope_len > limits.max_envelope_bytes || payload_len > limits.max_payload_bytes) {
        return core::make_error(core::ErrorCode::ResourceExhausted, "frame exceeds configured limits");
    }

    const auto expected_size = static_cast<std::size_t>(kHeaderLen) + envelope_len + payload_len;
    if (frame.size() != expected_size) {
        return core::make_error(core::ErrorCode::ProtocolError, "frame size does not match header lengths");
    }

    core::ByteBuffer metadata(frame.begin() + kHeaderLen, frame.begin() + kHeaderLen + envelope_len);
    auto decoded = deserialize_envelope_metadata(metadata);
    if (!decoded.ok()) {
        return decoded.error();
    }

    auto envelope = std::move(decoded).value();
    if (envelope.deadline_unix_ms > 0 && envelope.deadline_unix_ms < now_unix_ms) {
        return core::make_error(core::ErrorCode::DeadlineExceeded, "envelope deadline exceeded");
    }

    envelope.payload.assign(frame.begin() + kHeaderLen + envelope_len, frame.end());
    return envelope;
}

std::int64_t to_unix_ms(std::chrono::system_clock::time_point time_point) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()).count();
}

} 
