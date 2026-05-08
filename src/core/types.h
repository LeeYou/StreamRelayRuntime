#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace streamrelay::core {

using ConnectionId = std::uint64_t;
using Generation = std::uint32_t;
using RequestId = std::uint64_t;
using TraceId = std::uint64_t;
using ByteBuffer = std::vector<std::uint8_t>;

struct ConnectionRef {
    std::string gateway_id;
    ConnectionId connection_id{0};
    Generation generation{0};

    bool operator==(const ConnectionRef& other) const noexcept {
        return gateway_id == other.gateway_id &&
               connection_id == other.connection_id &&
               generation == other.generation;
    }

    bool operator!=(const ConnectionRef& other) const noexcept {
        return !(*this == other);
    }
};

} // namespace streamrelay::core
