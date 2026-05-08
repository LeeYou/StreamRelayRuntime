#pragma once

#include <chrono>
#include <string>

#include "core/result.h"
#include "core/types.h"
#include "device_registry/device_registry.h"
#include "protocol/envelope.h"
#include "router/service_registry.h"

namespace streamrelay::router {

enum class RouteKind {
    ServiceInstance,
    DeviceConnection,
};

struct RouteDecision {
    RouteKind kind{RouteKind::ServiceInstance};
    std::string service_name;
    std::string instance_id;
    std::string endpoint;
    std::string gateway_id;
    core::ConnectionRef connection;
};

class Router {
public:
    Router(InMemoryServiceRegistry& services, device_registry::InMemoryDeviceRegistry& devices);

    core::Result<RouteDecision> route_to_service(const protocol::Envelope& envelope, std::chrono::system_clock::time_point now) const;
    core::Result<RouteDecision> route_to_device(const std::string& tenant_id, const std::string& device_id, std::chrono::system_clock::time_point now) const;

private:
    InMemoryServiceRegistry& services_;
    device_registry::InMemoryDeviceRegistry& devices_;
};

} 
