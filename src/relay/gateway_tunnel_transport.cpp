#include "relay/gateway_tunnel_transport.h"

#include <utility>

namespace streamrelay::relay {

core::Result<void> InMemoryGatewayTunnelTransport::send(std::string source_gateway_id, std::string destination_gateway_id, core::ByteBuffer bytes) {
    if (source_gateway_id.empty() || destination_gateway_id.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "gateway tunnel transport endpoints are required");
    }
    if (bytes.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "gateway tunnel transport frame is empty");
    }

    stats_.frames_sent += 1;
    stats_.bytes_sent += bytes.size();
    queues_[route_key(destination_gateway_id, source_gateway_id)].push_back(std::move(bytes));
    return core::success();
}

core::Result<core::ByteBuffer> InMemoryGatewayTunnelTransport::receive(const std::string& destination_gateway_id, const std::string& source_gateway_id) {
    auto& queue = queues_[route_key(destination_gateway_id, source_gateway_id)];
    if (queue.empty()) {
        return core::make_error(core::ErrorCode::NotFound, "gateway tunnel transport queue is empty");
    }

    auto bytes = std::move(queue.front());
    queue.pop_front();
    stats_.frames_received += 1;
    stats_.bytes_received += bytes.size();
    return bytes;
}

std::size_t InMemoryGatewayTunnelTransport::pending(const std::string& destination_gateway_id, const std::string& source_gateway_id) const {
    const auto it = queues_.find(route_key(destination_gateway_id, source_gateway_id));
    if (it == queues_.end()) {
        return 0;
    }
    return it->second.size();
}

GatewayTunnelTransportStats InMemoryGatewayTunnelTransport::stats() const noexcept {
    return stats_;
}

std::string InMemoryGatewayTunnelTransport::route_key(const std::string& destination_gateway_id, const std::string& source_gateway_id) {
    return destination_gateway_id + "<-" + source_gateway_id;
}

} 
