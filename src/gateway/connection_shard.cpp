#include "gateway/connection_shard.h"

#include <utility>

namespace streamrelay::gateway {

ConnectionShard::ConnectionShard(ConnectionShardOptions options) : options_(std::move(options)) {
    if (options_.id_stride == 0) {
        options_.id_stride = 1;
    }

    next_connection_id_ = static_cast<core::ConnectionId>(options_.shard_index + 1);
}

core::Result<core::ConnectionRef> ConnectionShard::accept_connection(std::string protocol, std::string remote_address, std::chrono::steady_clock::time_point now) {
    const auto connection_id = next_connection_id_;
    next_connection_id_ += static_cast<core::ConnectionId>(options_.id_stride);
    core::ConnectionRef ref{options_.gateway_id, connection_id, 1};

    ConnectionInfo info;
    info.ref = ref;
    info.protocol = std::move(protocol);
    info.remote_address = std::move(remote_address);
    info.state = ConnectionState::Accepted;
    info.connected_at = now;
    info.last_read_at = now;
    info.last_write_at = now;

    ConnectionSlot slot{info, WriteQueue{options_.write_soft_limit_bytes, options_.write_hard_limit_bytes}};
    connections_.emplace(connection_id, std::move(slot));
    return ref;
}

core::Result<void> ConnectionShard::bind_connection(core::ConnectionRef ref, ClientKind kind) {
    auto slot = find_slot(ref);
    if (!slot.ok()) {
        return slot.error();
    }

    slot.value()->info.client_kind = kind;
    slot.value()->info.state = ConnectionState::Active;
    return core::success();
}

core::Result<void> ConnectionShard::mark_read(core::ConnectionRef ref, std::chrono::steady_clock::time_point now) {
    auto slot = find_slot(ref);
    if (!slot.ok()) {
        return slot.error();
    }

    slot.value()->info.last_read_at = now;
    return core::success();
}

core::Result<void> ConnectionShard::enqueue_write(core::ConnectionRef ref, core::ByteBuffer bytes, std::chrono::steady_clock::time_point now) {
    auto slot = find_slot(ref);
    if (!slot.ok()) {
        return slot.error();
    }

    auto result = slot.value()->write_queue.enqueue(std::move(bytes));
    if (!result.ok()) {
        slot.value()->info.state = ConnectionState::Closing;
        return result;
    }

    slot.value()->info.write_queue_bytes = slot.value()->write_queue.pending_bytes();
    slot.value()->info.last_write_at = now;
    if (slot.value()->write_queue.state() == WriteQueueState::SoftLimited) {
        slot.value()->info.state = ConnectionState::Backpressured;
    }

    return core::success();
}

core::Result<core::ByteBuffer> ConnectionShard::pop_write(core::ConnectionRef ref, std::chrono::steady_clock::time_point now) {
    auto slot = find_slot(ref);
    if (!slot.ok()) {
        return slot.error();
    }

    auto result = slot.value()->write_queue.pop_front();
    if (!result.ok()) {
        return result.error();
    }

    slot.value()->info.write_queue_bytes = slot.value()->write_queue.pending_bytes();
    slot.value()->info.last_write_at = now;
    if (slot.value()->info.state == ConnectionState::Backpressured && slot.value()->write_queue.state() == WriteQueueState::Normal) {
        slot.value()->info.state = ConnectionState::Active;
    }

    return std::move(result).value();
}

core::Result<void> ConnectionShard::close_connection(core::ConnectionRef ref) {
    auto slot = find_slot(ref);
    if (!slot.ok()) {
        return slot.error();
    }

    slot.value()->info.state = ConnectionState::Closed;
    slot.value()->write_queue.clear();
    slot.value()->info.write_queue_bytes = 0;
    return core::success();
}

void ConnectionShard::close_idle(std::chrono::steady_clock::time_point now) {
    for (auto& item : connections_) {
        auto& slot = item.second;
        if (slot.info.state == ConnectionState::Closed) {
            continue;
        }

        if (now - slot.info.last_read_at > options_.heartbeat_timeout) {
            slot.info.state = ConnectionState::Closed;
            slot.write_queue.clear();
            slot.info.write_queue_bytes = 0;
        }
    }
}

core::Result<ConnectionInfo> ConnectionShard::find_connection(core::ConnectionRef ref) const {
    auto slot = find_slot(ref);
    if (!slot.ok()) {
        return slot.error();
    }

    return slot.value()->info;
}

std::size_t ConnectionShard::active_connection_count() const noexcept {
    std::size_t count = 0;
    for (const auto& item : connections_) {
        if (item.second.info.state != ConnectionState::Closed) {
            ++count;
        }
    }
    return count;
}

std::size_t ConnectionShard::shard_index() const noexcept {
    return options_.shard_index;
}

core::Result<ConnectionShard::ConnectionSlot*> ConnectionShard::find_slot(core::ConnectionRef ref) {
    const auto it = connections_.find(ref.connection_id);
    if (it == connections_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "connection not found");
    }

    if (!matches_generation(it->second, ref)) {
        return core::make_error(core::ErrorCode::InvalidState, "stale connection generation");
    }

    return &it->second;
}

core::Result<const ConnectionShard::ConnectionSlot*> ConnectionShard::find_slot(core::ConnectionRef ref) const {
    const auto it = connections_.find(ref.connection_id);
    if (it == connections_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "connection not found");
    }

    if (!matches_generation(it->second, ref)) {
        return core::make_error(core::ErrorCode::InvalidState, "stale connection generation");
    }

    return &it->second;
}

bool ConnectionShard::matches_generation(const ConnectionSlot& slot, core::ConnectionRef ref) const noexcept {
    return slot.info.ref.gateway_id == ref.gateway_id &&
           slot.info.ref.connection_id == ref.connection_id &&
           slot.info.ref.generation == ref.generation;
}

} 
