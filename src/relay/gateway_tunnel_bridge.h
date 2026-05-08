#pragma once

#include <string>

#include "core/result.h"
#include "relay/gateway_tunnel.h"
#include "relay/gateway_tunnel_transport.h"
#include "relay/local_relay_data_plane.h"

namespace streamrelay::relay {

class GatewayTunnelBridge {
public:
    static core::Result<void> pump_once(GatewayTunnelManager& source, const std::string& source_remote_id, GatewayTunnelManager& destination, const std::string& destination_remote_id, const RelayFrameLimits& limits);
    static core::Result<void> pump_to_transport_once(GatewayTunnelManager& source, const std::string& source_remote_id, InMemoryGatewayTunnelTransport& transport, const std::string& source_gateway_id);
    static core::Result<void> pump_from_transport_once(InMemoryGatewayTunnelTransport& transport, const std::string& source_gateway_id, const std::string& destination_gateway_id, GatewayTunnelManager& destination, const std::string& destination_remote_id, const RelayFrameLimits& limits);
    static core::Result<void> deliver_once(GatewayTunnelManager& tunnel, const std::string& remote_gateway_id, LocalRelayDataPlane& data_plane, RelayEndpointKind from, std::chrono::steady_clock::time_point now);
};

} 
