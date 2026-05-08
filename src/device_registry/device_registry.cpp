#include "device_registry/device_registry.h"

#include <algorithm>
#include <utility>

namespace streamrelay::device_registry {

core::Result<void> InMemoryDeviceRegistry::register_device(DeviceMetadata metadata) {
    if (metadata.tenant_id.empty() || metadata.device_id.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "tenant_id and device_id are required");
    }

    const auto key = device_key(metadata.tenant_id, metadata.device_id);
    auto it = devices_.find(key);
    if (it == devices_.end()) {
        DeviceRecord record;
        record.metadata = std::move(metadata);
        devices_[key] = std::move(record);
    } else {
        it->second.metadata = std::move(metadata);
    }

    return core::success();
}

core::Result<void> InMemoryDeviceRegistry::update_heartbeat(const DeviceHeartbeat& heartbeat, std::chrono::system_clock::time_point now) {
    const auto key = device_key(heartbeat.tenant_id, heartbeat.device_id);
    auto it = devices_.find(key);
    if (it == devices_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "device not found");
    }

    it->second.presence.state = DevicePresenceState::Online;
    it->second.presence.connection = heartbeat.connection;
    it->second.presence.session_id = heartbeat.session_id;
    it->second.presence.last_seen_at = now;
    it->second.presence.expires_at = now + heartbeat.ttl;
    return core::success();
}

core::Result<DeviceRecord> InMemoryDeviceRegistry::find_device(const std::string& tenant_id, const std::string& device_id, std::chrono::system_clock::time_point now) const {
    const auto it = devices_.find(device_key(tenant_id, device_id));
    if (it == devices_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "device not found");
    }

    return visible_record(it->second, now);
}

core::Result<void> InMemoryDeviceRegistry::mark_offline(const std::string& tenant_id, const std::string& device_id, core::ConnectionRef connection) {
    auto it = devices_.find(device_key(tenant_id, device_id));
    if (it == devices_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "device not found");
    }

    if (!it->second.presence.connection.has_value()) {
        return core::make_error(core::ErrorCode::NotFound, "device is already offline");
    }

    if (it->second.presence.connection.value() != connection) {
        return core::make_error(core::ErrorCode::InvalidState, "stale device connection");
    }

    it->second.presence.state = DevicePresenceState::Offline;
    it->second.presence.connection.reset();
    it->second.presence.session_id.clear();
    return core::success();
}

void InMemoryDeviceRegistry::expire(std::chrono::system_clock::time_point now) {
    for (auto& item : devices_) {
        if (is_presence_expired(item.second.presence, now)) {
            item.second.presence.state = DevicePresenceState::Offline;
            item.second.presence.connection.reset();
            item.second.presence.session_id.clear();
        }
    }
}

std::vector<DeviceRecord> InMemoryDeviceRegistry::find_by_owner(const std::string& tenant_id, const std::string& owner_user_id, std::chrono::system_clock::time_point now) const {
    std::vector<DeviceRecord> out;
    for (const auto& item : devices_) {
        if (item.second.metadata.tenant_id == tenant_id && item.second.metadata.owner_user_id == owner_user_id) {
            out.push_back(visible_record(item.second, now));
        }
    }
    return out;
}

std::vector<DeviceRecord> InMemoryDeviceRegistry::find_by_capability(const std::string& tenant_id, const std::string& capability, std::chrono::system_clock::time_point now) const {
    std::vector<DeviceRecord> out;
    for (const auto& item : devices_) {
        if (item.second.metadata.tenant_id != tenant_id) {
            continue;
        }

        const auto& capabilities = item.second.metadata.capabilities;
        if (std::find(capabilities.begin(), capabilities.end(), capability) != capabilities.end()) {
            out.push_back(visible_record(item.second, now));
        }
    }
    return out;
}

std::size_t InMemoryDeviceRegistry::size() const noexcept {
    return devices_.size();
}

std::string InMemoryDeviceRegistry::device_key(const std::string& tenant_id, const std::string& device_id) {
    return tenant_id + ":" + device_id;
}

bool InMemoryDeviceRegistry::is_presence_expired(const DevicePresence& presence, std::chrono::system_clock::time_point now) const noexcept {
    return presence.state == DevicePresenceState::Online && presence.expires_at <= now;
}

DeviceRecord InMemoryDeviceRegistry::visible_record(DeviceRecord record, std::chrono::system_clock::time_point now) const {
    if (is_presence_expired(record.presence, now)) {
        record.presence.state = DevicePresenceState::Offline;
        record.presence.connection.reset();
        record.presence.session_id.clear();
    }
    return record;
}

} 
