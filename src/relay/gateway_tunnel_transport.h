#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>

#include "core/result.h"
#include "core/types.h"

namespace streamrelay::relay {

struct GatewayTunnelTransportStats {
    std::uint64_t frames_sent{0};
    std::uint64_t frames_received{0};
    std::uint64_t bytes_sent{0};
    std::uint64_t bytes_received{0};
};

class InMemoryGatewayTunnelTransport {
public:
    core::Result<void> send(std::string source_gateway_id, std::string destination_gateway_id, core::ByteBuffer bytes);
    core::Result<core::ByteBuffer> receive(const std::string& destination_gateway_id, const std::string& source_gateway_id);
    std::size_t pending(const std::string& destination_gateway_id, const std::string& source_gateway_id) const;
    GatewayTunnelTransportStats stats() const noexcept;

private:
    static std::string route_key(const std::string& destination_gateway_id, const std::string& source_gateway_id);

    std::unordered_map<std::string, std::deque<core::ByteBuffer>> queues_;
    GatewayTunnelTransportStats stats_;
};

} 
