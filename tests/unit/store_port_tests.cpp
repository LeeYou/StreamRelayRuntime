#include <cassert>
#include <chrono>

#include "device_registry/device_registry_store.h"
#include "session/session_store.h"

int main() {
    const auto now = std::chrono::system_clock::now();

    streamrelay::session::InMemorySessionStore session_store;
    streamrelay::session::SessionRecord session;
    session.session_id = "session-1";
    session.tenant_id = "tenant-1";
    session.principal_id = "user-1";
    session.kind = streamrelay::session::SessionKind::User;
    session.expires_at = now + std::chrono::seconds{5};
    auto put_session = session_store.put(session);
    assert(put_session.ok());
    assert(session_store.size() == 1);

    auto loaded_session = session_store.get("session-1");
    assert(loaded_session.ok());
    assert(loaded_session.value().principal_id == "user-1");

    streamrelay::core::ConnectionRef connection{"gateway-a", 7, 1};
    auto bound_session = session_store.bind_connection("session-1", connection);
    assert(bound_session.ok());
    auto by_connection = session_store.find_by_connection(connection);
    assert(by_connection.ok());
    assert(by_connection.value().session_id == "session-1");

    session_store.expire(now + std::chrono::seconds{6});
    assert(session_store.size() == 0);
    assert(!session_store.find_by_connection(connection).ok());

    streamrelay::device_registry::InMemoryDeviceRegistryStore device_store;
    streamrelay::device_registry::DeviceRecord device;
    device.metadata.tenant_id = "tenant-1";
    device.metadata.device_id = "device-1";
    device.metadata.owner_user_id = "user-1";
    device.metadata.capabilities = {"relay", "remote_control"};
    device.presence.state = streamrelay::device_registry::DevicePresenceState::Online;
    device.presence.connection = connection;
    device.presence.session_id = "session-2";
    device.presence.expires_at = now + std::chrono::seconds{5};
    auto put_device = device_store.put(device);
    assert(put_device.ok());
    assert(device_store.size() == 1);

    auto loaded_device = device_store.get("tenant-1", "device-1");
    assert(loaded_device.ok());
    assert(loaded_device.value().presence.connection.value() == connection);
    assert(device_store.find_by_owner("tenant-1", "user-1").size() == 1);
    assert(device_store.find_by_capability("tenant-1", "remote_control").size() == 1);

    device_store.expire_presence(now + std::chrono::seconds{6});
    loaded_device = device_store.get("tenant-1", "device-1");
    assert(loaded_device.ok());
    assert(loaded_device.value().presence.state == streamrelay::device_registry::DevicePresenceState::Offline);
    assert(!loaded_device.value().presence.connection.has_value());

    auto removed_device = device_store.remove("tenant-1", "device-1");
    assert(removed_device.ok());
    assert(device_store.size() == 0);
    return 0;
}
