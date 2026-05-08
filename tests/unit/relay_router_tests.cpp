#include <cassert>
#include <chrono>

#include "device_registry/device_registry.h"
#include "relay/gateway_tunnel.h"
#include "relay/relay_frame.h"
#include "router/router.h"
#include "router/service_registry.h"

int main() {
    const auto now = std::chrono::system_clock::now();
    streamrelay::router::InMemoryServiceRegistry services;
    streamrelay::router::ServiceInstance instance;
    instance.service_name = "control";
    instance.instance_id = "control-a";
    instance.endpoint = "127.0.0.1:1001";
    assert(services.register_instance(instance, now).ok());
    assert(services.select_best("control", now).ok());

    streamrelay::relay::GatewayTunnelManager a;
    streamrelay::relay::GatewayTunnelManager b;
    assert(a.connect("gateway-b").ok());
    assert(b.connect("gateway-a").ok());
    streamrelay::relay::RelayFrame frame;
    frame.header.channel_handle = 7;
    frame.payload = streamrelay::core::ByteBuffer{1, 2, 3};
    assert(a.send_frame("gateway-b", frame).ok());
    auto outbound = a.pop_outbound_frame("gateway-b");
    assert(outbound.ok());
    assert(b.receive_frame("gateway-a", outbound.value(), streamrelay::relay::RelayFrameLimits{}).ok());
    auto inbound = b.pop_inbound_frame("gateway-a");
    assert(inbound.ok());
    assert(inbound.value().payload == frame.payload);
    return 0;
}
