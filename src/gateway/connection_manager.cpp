#include "gateway/connection_manager.h"

#include <algorithm>
#include <utility>

namespace streamrelay::gateway {

ConnectionManager::ConnectionManager(ConnectionManagerOptions options) : options_(std::move(options)) {
    if (options_.shard_count == 0) {
        options_.shard_count = 1;
    }

    shards_.reserve(options_.shard_count);
    for (std::size_t i = 0; i < options_.shard_count; ++i) {
        ConnectionShardOptions shard_options;
        shard_options.gateway_id = options_.gateway_id;
        shard_options.shard_index = i;
        shard_options.write_soft_limit_bytes = options_.write_soft_limit_bytes;
        shard_options.write_hard_limit_bytes = options_.write_hard_limit_bytes;
        shard_options.heartbeat_timeout = options_.heartbeat_timeout;
        shards_.push_back(std::make_unique<ConnectionShard>(std::move(shard_options)));
    }
}

core::Result<core::ConnectionRef> ConnectionManager::accept_connection(std::string protocol, std::string remote_address, std::chrono::steady_clock::time_point now) {
    return next_accept_shard().accept_connection(std::move(protocol), std::move(remote_address), now);
}

core::Result<void> ConnectionManager::bind_connection(core::ConnectionRef ref, ClientKind kind) {
    return shard_for(ref).bind_connection(ref, kind);
}

core::Result<void> ConnectionManager::mark_read(core::ConnectionRef ref, std::chrono::steady_clock::time_point now) {
    return shard_for(ref).mark_read(ref, now);
}

core::Result<void> ConnectionManager::enqueue_write(core::ConnectionRef ref, core::ByteBuffer bytes, std::chrono::steady_clock::time_point now) {
    return shard_for(ref).enqueue_write(ref, std::move(bytes), now);
}

core::Result<core::ByteBuffer> ConnectionManager::pop_write(core::ConnectionRef ref, std::chrono::steady_clock::time_point now) {
    return shard_for(ref).pop_write(ref, now);
}

core::Result<void> ConnectionManager::close_connection(core::ConnectionRef ref) {
    return shard_for(ref).close_connection(ref);
}

void ConnectionManager::close_idle(std::chrono::steady_clock::time_point now) {
    for (auto& shard : shards_) {
        shard->close_idle(now);
    }
}

core::Result<ConnectionInfo> ConnectionManager::find_connection(core::ConnectionRef ref) const {
    return shard_for(ref).find_connection(ref);
}

std::size_t ConnectionManager::shard_count() const noexcept {
    return shards_.size();
}

std::size_t ConnectionManager::active_connection_count() const noexcept {
    std::size_t count = 0;
    for (const auto& shard : shards_) {
        count += shard->active_connection_count();
    }
    return count;
}

std::size_t ConnectionManager::shard_index_for(core::ConnectionRef ref) const noexcept {
    return static_cast<std::size_t>(ref.connection_id - 1) % shards_.size();
}

ConnectionShard& ConnectionManager::shard_for(core::ConnectionRef ref) noexcept {
    return *shards_[shard_index_for(ref)];
}

const ConnectionShard& ConnectionManager::shard_for(core::ConnectionRef ref) const noexcept {
    return *shards_[shard_index_for(ref)];
}

ConnectionShard& ConnectionManager::next_accept_shard() noexcept {
    auto& shard = *shards_[next_shard_index_];
    next_shard_index_ = (next_shard_index_ + 1) % shards_.size();
    return shard;
}

} 
