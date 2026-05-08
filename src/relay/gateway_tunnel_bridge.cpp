#include "relay/gateway_tunnel_bridge.h"

namespace streamrelay::relay {

core::Result<void> GatewayTunnelBridge::pump_once(GatewayTunnelManager& source, const std::string& source_remote_id, GatewayTunnelManager& destination, const std::string& destination_remote_id, const RelayFrameLimits& limits) {
    auto outbound = source.pop_outbound_frame(source_remote_id);
    if (!outbound.ok()) {
        return outbound.error();
    }
    return destination.receive_frame(destination_remote_id, outbound.value(), limits);
}

core::Result<void> GatewayTunnelBridge::pump_to_transport_once(GatewayTunnelManager& source, const std::string& source_remote_id, InMemoryGatewayTunnelTransport& transport, const std::string& source_gateway_id) {
    auto outbound = source.pop_outbound_frame(source_remote_id);
    if (!outbound.ok()) {
        return outbound.error();
    }
    return transport.send(source_gateway_id, source_remote_id, std::move(outbound).value());
}

core::Result<void> GatewayTunnelBridge::pump_from_transport_once(InMemoryGatewayTunnelTransport& transport, const std::string& source_gateway_id, const std::string& destination_gateway_id, GatewayTunnelManager& destination, const std::string& destination_remote_id, const RelayFrameLimits& limits) {
    auto encoded = transport.receive(destination_gateway_id, source_gateway_id);
    if (!encoded.ok()) {
        return encoded.error();
    }
    return destination.receive_frame(destination_remote_id, encoded.value(), limits);
}

core::Result<void> GatewayTunnelBridge::deliver_once(GatewayTunnelManager& tunnel, const std::string& remote_gateway_id, LocalRelayDataPlane& data_plane, RelayEndpointKind from, std::chrono::steady_clock::time_point now) {
    auto frame = tunnel.pop_inbound_frame(remote_gateway_id);
    if (!frame.ok()) {
        return frame.error();
    }
    return data_plane.forward_frame(frame.value(), from, now);
}

} 
