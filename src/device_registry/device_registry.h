#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/result.h"
#include "core/types.h"

namespace streamrelay::device_registry {

enum class DevicePresenceState {
    Offline,
    Online,
};

struct DeviceMetadata {
    std::string tenant_id;
    std::string device_id;
    std::string owner_user_id;
    std::vector<std::string> capabilities;
    std::unordered_map<std::string, std::string> labels;
};

struct DevicePresence {
    DevicePresenceState state{DevicePresenceState::Offline};
    std::optional<core::ConnectionRef> connection;
    std::string session_id;
    std::chrono::system_clock::time_point last_seen_at{};
    std::chrono::system_clock::time_point expires_at{};
};

struct DeviceRecord {
    DeviceMetadata metadata;
    DevicePresence presence;
};

struct DeviceHeartbeat {
    std::string tenant_id;
    std::string device_id;
    std::string session_id;
    core::ConnectionRef connection;
    std::chrono::seconds ttl{std::chrono::seconds(30)};
};

class InMemoryDeviceRegistry {
public:
    core::Result<void> register_device(DeviceMetadata metadata);
    core::Result<void> update_heartbeat(const DeviceHeartbeat& heartbeat, std::chrono::system_clock::time_point now);
    core::Result<DeviceRecord> find_device(const std::string& tenant_id, const std::string& device_id, std::chrono::system_clock::time_point now) const;
    core::Result<void> mark_offline(const std::string& tenant_id, const std::string& device_id, core::ConnectionRef connection);
    void expire(std::chrono::system_clock::time_point now);

    std::vector<DeviceRecord> find_by_owner(const std::string& tenant_id, const std::string& owner_user_id, std::chrono::system_clock::time_point now) const;
    std::vector<DeviceRecord> find_by_capability(const std::string& tenant_id, const std::string& capability, std::chrono::system_clock::time_point now) const;
    std::size_t size() const noexcept;

private:
    static std::string device_key(const std::string& tenant_id, const std::string& device_id);
    bool is_presence_expired(const DevicePresence& presence, std::chrono::system_clock::time_point now) const noexcept;
    DeviceRecord visible_record(DeviceRecord record, std::chrono::system_clock::time_point now) const;

    std::unordered_map<std::string, DeviceRecord> devices_;
};

} 
