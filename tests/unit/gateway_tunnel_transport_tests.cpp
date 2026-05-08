#include <cassert>

#include "relay/gateway_tunnel_bridge.h"
#include "relay/gateway_tunnel_transport.h"

int main() {
    streamrelay::relay::GatewayTunnelManager gateway_a;
    streamrelay::relay::GatewayTunnelManager gateway_b;
    auto a_connected = gateway_a.connect("gateway-b");
    assert(a_connected.ok());
    auto b_connected = gateway_b.connect("gateway-a");
    assert(b_connected.ok());

    streamrelay::relay::InMemoryGatewayTunnelTransport transport;
    streamrelay::relay::RelayFrame outbound_a;
    outbound_a.header.channel_handle = 11;
    outbound_a.header.sequence = 1;
    outbound_a.payload = streamrelay::core::ByteBuffer{1, 2, 3, 4};
    auto sent_a = gateway_a.send_frame("gateway-b", outbound_a);
    assert(sent_a.ok());

    auto pumped_to_transport = streamrelay::relay::GatewayTunnelBridge::pump_to_transport_once(gateway_a, "gateway-b", transport, "gateway-a");
    assert(pumped_to_transport.ok());
    assert(transport.pending("gateway-b", "gateway-a") == 1);

    auto pumped_to_gateway_b = streamrelay::relay::GatewayTunnelBridge::pump_from_transport_once(transport, "gateway-a", "gateway-b", gateway_b, "gateway-a", streamrelay::relay::RelayFrameLimits{});
    assert(pumped_to_gateway_b.ok());
    assert(transport.pending("gateway-b", "gateway-a") == 0);
    auto inbound_b = gateway_b.pop_inbound_frame("gateway-a");
    assert(inbound_b.ok());
    assert(inbound_b.value().header.channel_handle == 11);
    assert(inbound_b.value().header.sequence == 1);
    assert(inbound_b.value().payload == outbound_a.payload);

    streamrelay::relay::RelayFrame outbound_b;
    outbound_b.header.channel_handle = 11;
    outbound_b.header.sequence = 2;
    outbound_b.header.ack_sequence = 1;
    outbound_b.payload = streamrelay::core::ByteBuffer{9, 8};
    auto sent_b = gateway_b.send_frame("gateway-a", outbound_b);
    assert(sent_b.ok());
    auto pumped_reply_to_transport = streamrelay::relay::GatewayTunnelBridge::pump_to_transport_once(gateway_b, "gateway-a", transport, "gateway-b");
    assert(pumped_reply_to_transport.ok());
    auto pumped_reply_to_gateway_a = streamrelay::relay::GatewayTunnelBridge::pump_from_transport_once(transport, "gateway-b", "gateway-a", gateway_a, "gateway-b", streamrelay::relay::RelayFrameLimits{});
    assert(pumped_reply_to_gateway_a.ok());
    auto inbound_a = gateway_a.pop_inbound_frame("gateway-b");
    assert(inbound_a.ok());
    assert(inbound_a.value().header.sequence == 2);
    assert(inbound_a.value().header.ack_sequence == 1);
    assert(inbound_a.value().payload == outbound_b.payload);

    auto transport_stats = transport.stats();
    assert(transport_stats.frames_sent == 2);
    assert(transport_stats.frames_received == 2);
    assert(transport_stats.bytes_sent > 0);
    assert(transport_stats.bytes_received == transport_stats.bytes_sent);

    auto a_stats = gateway_a.stats("gateway-b");
    assert(a_stats.ok());
    assert(a_stats.value().frames_sent == 1);
    assert(a_stats.value().frames_received == 1);
    auto b_stats = gateway_b.stats("gateway-a");
    assert(b_stats.ok());
    assert(b_stats.value().frames_sent == 1);
    assert(b_stats.value().frames_received == 1);

    auto empty_receive = transport.receive("gateway-a", "gateway-b");
    assert(!empty_receive.ok());
    assert(empty_receive.error().code == streamrelay::core::ErrorCode::NotFound);

    return 0;
}
