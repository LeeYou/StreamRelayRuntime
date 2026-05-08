#pragma once

#include <chrono>
#include <cstddef>
#include <string>

#include "core/types.h"

namespace streamrelay::gateway {

enum class ConnectionState {
    Accepted,
    Authenticating,
    Bound,
    Active,
    Backpressured,
    Closing,
    Closed,
};

enum class ClientKind {
    Unknown,
    Admin,
    DeviceAgent,
};

struct ConnectionInfo {
    core::ConnectionRef ref;
    std::string protocol;
    std::string remote_address;
    ClientKind client_kind{ClientKind::Unknown};
    ConnectionState state{ConnectionState::Accepted};
    std::chrono::steady_clock::time_point connected_at{};
    std::chrono::steady_clock::time_point last_read_at{};
    std::chrono::steady_clock::time_point last_write_at{};
    std::size_t write_queue_bytes{0};
};

} 
