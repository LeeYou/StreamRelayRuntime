#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <unordered_map>

#include "core/result.h"
#include "core/types.h"
#include "gateway/connection.h"
#include "gateway/write_queue.h"

namespace streamrelay::gateway {

struct ConnectionShardOptions {
    std::string gateway_id;
    std::size_t shard_index{0};
    std::size_t id_stride{1};
    std::size_t write_soft_limit_bytes{64 * 1024};
    std::size_t write_hard_limit_bytes{256 * 1024};
    std::chrono::milliseconds heartbeat_timeout{std::chrono::seconds(30)};
};

class ConnectionShard {
public:
    explicit ConnectionShard(ConnectionShardOptions options);

    core::Result<core::ConnectionRef> accept_connection(std::string protocol, std::string remote_address, std::chrono::steady_clock::time_point now);
    core::Result<void> bind_connection(core::ConnectionRef ref, ClientKind kind);
    core::Result<void> mark_read(core::ConnectionRef ref, std::chrono::steady_clock::time_point now);
    core::Result<void> enqueue_write(core::ConnectionRef ref, core::ByteBuffer bytes, std::chrono::steady_clock::time_point now);
    core::Result<core::ByteBuffer> pop_write(core::ConnectionRef ref, std::chrono::steady_clock::time_point now);
    core::Result<void> close_connection(core::ConnectionRef ref);
    void close_idle(std::chrono::steady_clock::time_point now);

    core::Result<ConnectionInfo> find_connection(core::ConnectionRef ref) const;
    std::size_t active_connection_count() const noexcept;
    std::size_t shard_index() const noexcept;

private:
    struct ConnectionSlot {
        ConnectionInfo info;
        WriteQueue write_queue;
    };

    core::Result<ConnectionSlot*> find_slot(core::ConnectionRef ref);
    core::Result<const ConnectionSlot*> find_slot(core::ConnectionRef ref) const;
    bool matches_generation(const ConnectionSlot& slot, core::ConnectionRef ref) const noexcept;

    ConnectionShardOptions options_;
    core::ConnectionId next_connection_id_{1};
    std::unordered_map<core::ConnectionId, ConnectionSlot> connections_;
};

} 
