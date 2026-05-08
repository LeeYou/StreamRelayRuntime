#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/result.h"
#include "device_registry/device_registry.h"

namespace streamrelay::device_registry {

class IDeviceRegistryStore {
public:
    virtual ~IDeviceRegistryStore() = default;

    virtual core::Result<void> put(DeviceRecord record) = 0;
    virtual core::Result<DeviceRecord> get(const std::string& tenant_id, const std::string& device_id) const = 0;
    virtual core::Result<void> remove(const std::string& tenant_id, const std::string& device_id) = 0;
    virtual std::vector<DeviceRecord> find_by_owner(const std::string& tenant_id, const std::string& owner_user_id) const = 0;
    virtual std::vector<DeviceRecord> find_by_capability(const std::string& tenant_id, const std::string& capability) const = 0;
    virtual void expire_presence(std::chrono::system_clock::time_point now) = 0;
    virtual std::size_t size() const noexcept = 0;
};

class InMemoryDeviceRegistryStore final : public IDeviceRegistryStore {
public:
    core::Result<void> put(DeviceRecord record) override;
    core::Result<DeviceRecord> get(const std::string& tenant_id, const std::string& device_id) const override;
    core::Result<void> remove(const std::string& tenant_id, const std::string& device_id) override;
    std::vector<DeviceRecord> find_by_owner(const std::string& tenant_id, const std::string& owner_user_id) const override;
    std::vector<DeviceRecord> find_by_capability(const std::string& tenant_id, const std::string& capability) const override;
    void expire_presence(std::chrono::system_clock::time_point now) override;
    std::size_t size() const noexcept override;

private:
    static std::string device_key(const std::string& tenant_id, const std::string& device_id);
    static bool is_presence_expired(const DevicePresence& presence, std::chrono::system_clock::time_point now) noexcept;

    std::unordered_map<std::string, DeviceRecord> devices_;
};

} 
