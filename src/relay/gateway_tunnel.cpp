#include "relay/gateway_tunnel.h"

#include <utility>

namespace streamrelay::relay {

core::Result<void> GatewayTunnelManager::connect(std::string remote_gateway_id) {
    if (remote_gateway_id.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "remote_gateway_id is required");
    }

    auto& tunnel = tunnels_[remote_gateway_id];
    tunnel.remote_gateway_id = std::move(remote_gateway_id);
    tunnel.state = GatewayTunnelState::Connected;
    return core::success();
}

core::Result<void> GatewayTunnelManager::close(const std::string& remote_gateway_id) {
    auto it = tunnels_.find(remote_gateway_id);
    if (it == tunnels_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "gateway tunnel not found");
    }

    it->second.state = GatewayTunnelState::Closed;
    it->second.outbound_frames.clear();
    it->second.inbound_frames.clear();
    return core::success();
}

core::Result<void> GatewayTunnelManager::send_frame(const std::string& remote_gateway_id, const RelayFrame& frame) {
    auto tunnel = find_connected(remote_gateway_id);
    if (!tunnel.ok()) {
        return tunnel.error();
    }

    auto encoded = encode_relay_frame(frame);
    if (!encoded.ok()) {
        return encoded.error();
    }

    tunnel.value()->stats.frames_sent += 1;
    tunnel.value()->stats.bytes_sent += encoded.value().size();
    tunnel.value()->outbound_frames.push_back(std::move(encoded).value());
    return core::success();
}

core::Result<core::ByteBuffer> GatewayTunnelManager::pop_outbound_frame(const std::string& remote_gateway_id) {
    auto tunnel = find_connected(remote_gateway_id);
    if (!tunnel.ok()) {
        return tunnel.error();
    }

    if (tunnel.value()->outbound_frames.empty()) {
        return core::make_error(core::ErrorCode::NotFound, "outbound tunnel queue is empty");
    }

    auto frame = std::move(tunnel.value()->outbound_frames.front());
    tunnel.value()->outbound_frames.pop_front();
    return frame;
}

core::Result<void> GatewayTunnelManager::receive_frame(const std::string& remote_gateway_id, const core::ByteBuffer& encoded, const RelayFrameLimits& limits) {
    auto tunnel = find_connected(remote_gateway_id);
    if (!tunnel.ok()) {
        return tunnel.error();
    }

    auto frame = decode_relay_frame(encoded, limits);
    if (!frame.ok()) {
        return frame.error();
    }

    tunnel.value()->stats.frames_received += 1;
    tunnel.value()->stats.bytes_received += encoded.size();
    tunnel.value()->inbound_frames.push_back(std::move(frame).value());
    return core::success();
}

core::Result<RelayFrame> GatewayTunnelManager::pop_inbound_frame(const std::string& remote_gateway_id) {
    auto tunnel = find_connected(remote_gateway_id);
    if (!tunnel.ok()) {
        return tunnel.error();
    }

    if (tunnel.value()->inbound_frames.empty()) {
        return core::make_error(core::ErrorCode::NotFound, "inbound tunnel queue is empty");
    }

    auto frame = std::move(tunnel.value()->inbound_frames.front());
    tunnel.value()->inbound_frames.pop_front();
    return frame;
}

core::Result<GatewayTunnelStats> GatewayTunnelManager::stats(const std::string& remote_gateway_id) const {
    auto tunnel = find_connected(remote_gateway_id);
    if (!tunnel.ok()) {
        return tunnel.error();
    }

    return tunnel.value()->stats;
}

std::size_t GatewayTunnelManager::tunnel_count() const noexcept {
    return tunnels_.size();
}

core::Result<GatewayTunnel*> GatewayTunnelManager::find_connected(const std::string& remote_gateway_id) {
    auto it = tunnels_.find(remote_gateway_id);
    if (it == tunnels_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "gateway tunnel not found");
    }

    if (it->second.state != GatewayTunnelState::Connected) {
        return core::make_error(core::ErrorCode::InvalidState, "gateway tunnel is not connected");
    }

    return &it->second;
}

core::Result<const GatewayTunnel*> GatewayTunnelManager::find_connected(const std::string& remote_gateway_id) const {
    auto it = tunnels_.find(remote_gateway_id);
    if (it == tunnels_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "gateway tunnel not found");
    }

    if (it->second.state != GatewayTunnelState::Connected) {
        return core::make_error(core::ErrorCode::InvalidState, "gateway tunnel is not connected");
    }

    return &it->second;
}

} 
