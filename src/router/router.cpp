#include "router/router.h"

namespace streamrelay::router {

Router::Router(InMemoryServiceRegistry& services, device_registry::InMemoryDeviceRegistry& devices)
    : services_(services), devices_(devices) {}

core::Result<RouteDecision> Router::route_to_service(const protocol::Envelope& envelope, std::chrono::system_clock::time_point now) const {
    if (envelope.service.empty() || envelope.method.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "service and method are required");
    }

    auto selected = services_.select_best(envelope.service, now);
    if (!selected.ok()) {
        return selected.error();
    }

    RouteDecision decision;
    decision.kind = RouteKind::ServiceInstance;
    decision.service_name = selected.value().service_name;
    decision.instance_id = selected.value().instance_id;
    decision.endpoint = selected.value().endpoint;
    return decision;
}

core::Result<RouteDecision> Router::route_to_device(const std::string& tenant_id, const std::string& device_id, std::chrono::system_clock::time_point now) const {
    auto device = devices_.find_device(tenant_id, device_id, now);
    if (!device.ok()) {
        return device.error();
    }

    if (device.value().presence.state != device_registry::DevicePresenceState::Online || !device.value().presence.connection.has_value()) {
        return core::make_error(core::ErrorCode::NotFound, "device is offline");
    }

    RouteDecision decision;
    decision.kind = RouteKind::DeviceConnection;
    decision.gateway_id = device.value().presence.connection.value().gateway_id;
    decision.connection = device.value().presence.connection.value();
    return decision;
}

} 
