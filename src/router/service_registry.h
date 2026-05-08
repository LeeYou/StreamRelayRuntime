#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/result.h"

namespace streamrelay::router {

struct ServiceInstance {
    std::string service_name;
    std::string instance_id;
    std::string endpoint;
    std::string locality;
    bool healthy{true};
    std::uint64_t generation{1};
    std::uint32_t queue_depth{0};
    std::uint32_t inflight_requests{0};
    std::uint32_t p95_latency_ms{0};
    std::chrono::system_clock::time_point last_heartbeat_at{};
    std::chrono::milliseconds ttl{std::chrono::seconds(30)};
};

class InMemoryServiceRegistry {
public:
    core::Result<ServiceInstance> register_instance(ServiceInstance instance, std::chrono::system_clock::time_point now);
    core::Result<void> update_health(const std::string& service_name, const std::string& instance_id, bool healthy, std::chrono::system_clock::time_point now);
    core::Result<void> update_load(const std::string& service_name, const std::string& instance_id, std::uint32_t queue_depth, std::uint32_t inflight_requests, std::uint32_t p95_latency_ms);
    core::Result<ServiceInstance> get_instance(const std::string& service_name, const std::string& instance_id, std::chrono::system_clock::time_point now) const;
    std::vector<ServiceInstance> list_healthy(const std::string& service_name, std::chrono::system_clock::time_point now) const;
    core::Result<ServiceInstance> select_best(const std::string& service_name, std::chrono::system_clock::time_point now) const;
    std::size_t size() const noexcept;

private:
    static std::string instance_key(const std::string& service_name, const std::string& instance_id);
    bool is_live(const ServiceInstance& instance, std::chrono::system_clock::time_point now) const noexcept;
    std::uint64_t score(const ServiceInstance& instance) const noexcept;

    std::unordered_map<std::string, ServiceInstance> instances_;
};

} 
