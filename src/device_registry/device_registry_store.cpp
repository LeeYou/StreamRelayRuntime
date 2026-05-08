#include "device_registry/device_registry_store.h"

#include <algorithm>
#include <utility>

namespace streamrelay::device_registry {

core::Result<void> InMemoryDeviceRegistryStore::put(DeviceRecord record) {
    if (record.metadata.tenant_id.empty() || record.metadata.device_id.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "tenant_id and device_id are required");
    }
    devices_[device_key(record.metadata.tenant_id, record.metadata.device_id)] = std::move(record);
    return core::success();
}

core::Result<DeviceRecord> InMemoryDeviceRegistryStore::get(const std::string& tenant_id, const std::string& device_id) const {
    const auto it = devices_.find(device_key(tenant_id, device_id));
    if (it == devices_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "device not found");
    }
    return it->second;
}

core::Result<void> InMemoryDeviceRegistryStore::remove(const std::string& tenant_id, const std::string& device_id) {
    const auto removed = devices_.erase(device_key(tenant_id, device_id));
    if (removed == 0) {
        return core::make_error(core::ErrorCode::NotFound, "device not found");
    }
    return core::success();
}

std::vector<DeviceRecord> InMemoryDeviceRegistryStore::find_by_owner(const std::string& tenant_id, const std::string& owner_user_id) const {
    std::vector<DeviceRecord> out;
    for (const auto& item : devices_) {
        if (item.second.metadata.tenant_id == tenant_id && item.second.metadata.owner_user_id == owner_user_id) {
            out.push_back(item.second);
        }
    }
    return out;
}

std::vector<DeviceRecord> InMemoryDeviceRegistryStore::find_by_capability(const std::string& tenant_id, const std::string& capability) const {
    std::vector<DeviceRecord> out;
    for (const auto& item : devices_) {
        if (item.second.metadata.tenant_id != tenant_id) {
            continue;
        }
        const auto& capabilities = item.second.metadata.capabilities;
        if (std::find(capabilities.begin(), capabilities.end(), capability) != capabilities.end()) {
            out.push_back(item.second);
        }
    }
    return out;
}

void InMemoryDeviceRegistryStore::expire_presence(std::chrono::system_clock::time_point now) {
    for (auto& item : devices_) {
        if (is_presence_expired(item.second.presence, now)) {
            item.second.presence.state = DevicePresenceState::Offline;
            item.second.presence.connection.reset();
            item.second.presence.session_id.clear();
        }
    }
}

std::size_t InMemoryDeviceRegistryStore::size() const noexcept {
    return devices_.size();
}

std::string InMemoryDeviceRegistryStore::device_key(const std::string& tenant_id, const std::string& device_id) {
    return tenant_id + ":" + device_id;
}

bool InMemoryDeviceRegistryStore::is_presence_expired(const DevicePresence& presence, std::chrono::system_clock::time_point now) noexcept {
    return presence.state == DevicePresenceState::Online && presence.expires_at <= now;
}

} 
