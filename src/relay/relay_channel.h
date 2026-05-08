#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

#include "core/result.h"
#include "core/types.h"
#include "relay/relay_frame.h"

namespace streamrelay::relay {

enum class RelayChannelState {
    Creating,
    Active,
    Backpressured,
    Draining,
    Reset,
    Closed,
};

enum class RelayEndpointKind {
    Source,
    Target,
};

struct RelayChannelOptions {
    std::size_t soft_limit_bytes{64 * 1024};
    std::size_t hard_limit_bytes{256 * 1024};
};

struct RelayChannelStats {
    std::uint64_t frames_from_source{0};
    std::uint64_t frames_from_target{0};
    std::uint64_t bytes_from_source{0};
    std::uint64_t bytes_from_target{0};
    std::uint64_t backpressure_events{0};
};

struct RelayChannelRecord {
    ChannelHandle channel_handle{0};
    std::string tenant_id;
    std::string source_session_id;
    std::string target_device_id;
    core::ConnectionRef source_connection;
    core::ConnectionRef target_connection;
    RelayChannelState state{RelayChannelState::Creating};
    RelayChannelOptions options;
    RelayChannelStats stats;
    std::string close_reason;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point closed_at{};
};

struct OpenRelayChannelRequest {
    std::string tenant_id;
    std::string source_session_id;
    std::string target_device_id;
    core::ConnectionRef source_connection;
    core::ConnectionRef target_connection;
    RelayChannelOptions options;
};

class RelayChannelManager {
public:
    core::Result<RelayChannelRecord> open_channel(const OpenRelayChannelRequest& request, std::chrono::system_clock::time_point now);
    core::Result<void> close_channel(ChannelHandle handle, const std::string& reason, std::chrono::system_clock::time_point now);
    core::Result<RelayChannelRecord> get_channel(ChannelHandle handle) const;
    core::Result<void> record_forward(ChannelHandle handle, RelayEndpointKind from, std::size_t payload_bytes);
    core::Result<void> set_backpressured(ChannelHandle handle, bool backpressured);
    std::size_t active_channel_count() const noexcept;

private:
    ChannelHandle next_handle_{1};
    std::unordered_map<ChannelHandle, RelayChannelRecord> channels_;
};

core::ConnectionRef opposite_endpoint(const RelayChannelRecord& channel, RelayEndpointKind from);

} 
