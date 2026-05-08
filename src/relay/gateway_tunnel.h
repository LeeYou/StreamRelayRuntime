#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>

#include "core/result.h"
#include "core/types.h"
#include "relay/relay_frame.h"

namespace streamrelay::relay {

enum class GatewayTunnelState {
    Connected,
    Closed,
};

struct GatewayTunnelStats {
    std::uint64_t frames_sent{0};
    std::uint64_t frames_received{0};
    std::uint64_t bytes_sent{0};
    std::uint64_t bytes_received{0};
};

struct GatewayTunnel {
    std::string remote_gateway_id;
    GatewayTunnelState state{GatewayTunnelState::Connected};
    GatewayTunnelStats stats;
    std::deque<core::ByteBuffer> outbound_frames;
    std::deque<RelayFrame> inbound_frames;
};

class GatewayTunnelManager {
public:
    core::Result<void> connect(std::string remote_gateway_id);
    core::Result<void> close(const std::string& remote_gateway_id);
    core::Result<void> send_frame(const std::string& remote_gateway_id, const RelayFrame& frame);
    core::Result<core::ByteBuffer> pop_outbound_frame(const std::string& remote_gateway_id);
    core::Result<void> receive_frame(const std::string& remote_gateway_id, const core::ByteBuffer& encoded, const RelayFrameLimits& limits);
    core::Result<RelayFrame> pop_inbound_frame(const std::string& remote_gateway_id);
    core::Result<GatewayTunnelStats> stats(const std::string& remote_gateway_id) const;
    std::size_t tunnel_count() const noexcept;

private:
    core::Result<GatewayTunnel*> find_connected(const std::string& remote_gateway_id);
    core::Result<const GatewayTunnel*> find_connected(const std::string& remote_gateway_id) const;

    std::unordered_map<std::string, GatewayTunnel> tunnels_;
};

} 
