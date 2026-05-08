#include "router/service_registry.h"

#include <limits>
#include <utility>

namespace streamrelay::router {

core::Result<ServiceInstance> InMemoryServiceRegistry::register_instance(ServiceInstance instance, std::chrono::system_clock::time_point now) {
    if (instance.service_name.empty() || instance.instance_id.empty() || instance.endpoint.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "service_name, instance_id and endpoint are required");
    }

    const auto key = instance_key(instance.service_name, instance.instance_id);
    const auto existing = instances_.find(key);
    if (existing != instances_.end()) {
        instance.generation = existing->second.generation + 1;
    }

    instance.last_heartbeat_at = now;
    instances_[key] = std::move(instance);
    return instances_[key];
}

core::Result<void> InMemoryServiceRegistry::update_health(const std::string& service_name, const std::string& instance_id, bool healthy, std::chrono::system_clock::time_point now) {
    auto it = instances_.find(instance_key(service_name, instance_id));
    if (it == instances_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "service instance not found");
    }

    it->second.healthy = healthy;
    it->second.last_heartbeat_at = now;
    return core::success();
}

core::Result<void> InMemoryServiceRegistry::update_load(const std::string& service_name, const std::string& instance_id, std::uint32_t queue_depth, std::uint32_t inflight_requests, std::uint32_t p95_latency_ms) {
    auto it = instances_.find(instance_key(service_name, instance_id));
    if (it == instances_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "service instance not found");
    }

    it->second.queue_depth = queue_depth;
    it->second.inflight_requests = inflight_requests;
    it->second.p95_latency_ms = p95_latency_ms;
    return core::success();
}

core::Result<ServiceInstance> InMemoryServiceRegistry::get_instance(const std::string& service_name, const std::string& instance_id, std::chrono::system_clock::time_point now) const {
    const auto it = instances_.find(instance_key(service_name, instance_id));
    if (it == instances_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "service instance not found");
    }

    if (!is_live(it->second, now)) {
        return core::make_error(core::ErrorCode::DeadlineExceeded, "service instance heartbeat expired");
    }

    return it->second;
}

std::vector<ServiceInstance> InMemoryServiceRegistry::list_healthy(const std::string& service_name, std::chrono::system_clock::time_point now) const {
    std::vector<ServiceInstance> out;
    for (const auto& item : instances_) {
        if (item.second.service_name == service_name && is_live(item.second, now)) {
            out.push_back(item.second);
        }
    }
    return out;
}

core::Result<ServiceInstance> InMemoryServiceRegistry::select_best(const std::string& service_name, std::chrono::system_clock::time_point now) const {
    const auto candidates = list_healthy(service_name, now);
    if (candidates.empty()) {
        return core::make_error(core::ErrorCode::NotFound, "no healthy service instance found");
    }

    ServiceInstance best = candidates.front();
    auto best_score = score(best);
    for (const auto& candidate : candidates) {
        const auto candidate_score = score(candidate);
        if (candidate_score < best_score || (candidate_score == best_score && candidate.instance_id < best.instance_id)) {
            best = candidate;
            best_score = candidate_score;
        }
    }

    return best;
}

std::size_t InMemoryServiceRegistry::size() const noexcept {
    return instances_.size();
}

std::string InMemoryServiceRegistry::instance_key(const std::string& service_name, const std::string& instance_id) {
    return service_name + ":" + instance_id;
}

bool InMemoryServiceRegistry::is_live(const ServiceInstance& instance, std::chrono::system_clock::time_point now) const noexcept {
    return instance.healthy && now - instance.last_heartbeat_at <= instance.ttl;
}

std::uint64_t InMemoryServiceRegistry::score(const ServiceInstance& instance) const noexcept {
    return static_cast<std::uint64_t>(instance.queue_depth) * 1000ULL +
           static_cast<std::uint64_t>(instance.inflight_requests) * 100ULL +
           static_cast<std::uint64_t>(instance.p95_latency_ms);
}

} 
