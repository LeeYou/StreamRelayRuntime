#include <cassert>
#include <chrono>

#include "device_registry/device_registry.h"
#include "session/session_service.h"

int main() {
    const auto now = std::chrono::system_clock::now();

    streamrelay::session::InMemorySessionService sessions;
    streamrelay::session::CreateSessionRequest request;
    request.tenant_id = "tenant-1";
    request.principal_id = "device-1";
    request.ttl = std::chrono::seconds{5};
    auto session_id = sessions.create_device_session(request, now);
    assert(session_id.ok());
    streamrelay::core::ConnectionRef ref{"gateway-a", 1, 1};
    assert(sessions.bind_connection(session_id.value(), ref, now).ok());
    assert(sessions.find_by_connection(ref, now).ok());

    streamrelay::device_registry::InMemoryDeviceRegistry registry;
    streamrelay::device_registry::DeviceMetadata metadata;
    metadata.tenant_id = "tenant-1";
    metadata.device_id = "device-1";
    metadata.owner_user_id = "user-1";
    assert(registry.register_device(metadata).ok());
    streamrelay::device_registry::DeviceHeartbeat heartbeat;
    heartbeat.tenant_id = "tenant-1";
    heartbeat.device_id = "device-1";
    heartbeat.session_id = session_id.value();
    heartbeat.connection = ref;
    heartbeat.ttl = std::chrono::seconds{5};
    assert(registry.update_heartbeat(heartbeat, now).ok());
    auto found = registry.find_device("tenant-1", "device-1", now);
    assert(found.ok());
    assert(found.value().presence.state == streamrelay::device_registry::DevicePresenceState::Online);
    return 0;
}
