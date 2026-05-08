#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "core/result.h"
#include "core/types.h"
#include "gateway/connection.h"
#include "gateway/connection_shard.h"

namespace streamrelay::gateway {

struct ConnectionManagerOptions {
    std::string gateway_id;
    std::size_t shard_count{1};
    std::size_t write_soft_limit_bytes{64 * 1024};
    std::size_t write_hard_limit_bytes{256 * 1024};
    std::chrono::milliseconds heartbeat_timeout{std::chrono::seconds(30)};
};

class ConnectionManager {
public:
    explicit ConnectionManager(ConnectionManagerOptions options);

    core::Result<core::ConnectionRef> accept_connection(std::string protocol, std::string remote_address, std::chrono::steady_clock::time_point now);
    core::Result<void> bind_connection(core::ConnectionRef ref, ClientKind kind);
    core::Result<void> mark_read(core::ConnectionRef ref, std::chrono::steady_clock::time_point now);
    core::Result<void> enqueue_write(core::ConnectionRef ref, core::ByteBuffer bytes, std::chrono::steady_clock::time_point now);
    core::Result<core::ByteBuffer> pop_write(core::ConnectionRef ref, std::chrono::steady_clock::time_point now);
    core::Result<void> close_connection(core::ConnectionRef ref);
    void close_idle(std::chrono::steady_clock::time_point now);

    core::Result<ConnectionInfo> find_connection(core::ConnectionRef ref) const;
    std::size_t shard_count() const noexcept;
    std::size_t active_connection_count() const noexcept;
    std::size_t shard_index_for(core::ConnectionRef ref) const noexcept;

private:
    ConnectionShard& shard_for(core::ConnectionRef ref) noexcept;
    const ConnectionShard& shard_for(core::ConnectionRef ref) const noexcept;
    ConnectionShard& next_accept_shard() noexcept;

    ConnectionManagerOptions options_;
    std::vector<std::unique_ptr<ConnectionShard>> shards_;
    std::size_t next_shard_index_{0};
};

} 
