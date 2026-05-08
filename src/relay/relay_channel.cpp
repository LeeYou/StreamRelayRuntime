#include "relay/relay_channel.h"

namespace streamrelay::relay {

core::Result<RelayChannelRecord> RelayChannelManager::open_channel(const OpenRelayChannelRequest& request, std::chrono::system_clock::time_point now) {
    if (request.tenant_id.empty() || request.source_session_id.empty() || request.target_device_id.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "tenant_id, source_session_id and target_device_id are required");
    }

    if (request.options.soft_limit_bytes > request.options.hard_limit_bytes) {
        return core::make_error(core::ErrorCode::InvalidArgument, "soft limit cannot exceed hard limit");
    }

    RelayChannelRecord record;
    record.channel_handle = next_handle_++;
    record.tenant_id = request.tenant_id;
    record.source_session_id = request.source_session_id;
    record.target_device_id = request.target_device_id;
    record.source_connection = request.source_connection;
    record.target_connection = request.target_connection;
    record.options = request.options;
    record.state = RelayChannelState::Active;
    record.created_at = now;

    channels_[record.channel_handle] = record;
    return record;
}

core::Result<void> RelayChannelManager::close_channel(ChannelHandle handle, const std::string& reason, std::chrono::system_clock::time_point now) {
    auto it = channels_.find(handle);
    if (it == channels_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "relay channel not found");
    }

    it->second.state = RelayChannelState::Closed;
    it->second.close_reason = reason;
    it->second.closed_at = now;
    return core::success();
}

core::Result<RelayChannelRecord> RelayChannelManager::get_channel(ChannelHandle handle) const {
    const auto it = channels_.find(handle);
    if (it == channels_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "relay channel not found");
    }

    return it->second;
}

core::Result<void> RelayChannelManager::record_forward(ChannelHandle handle, RelayEndpointKind from, std::size_t payload_bytes) {
    auto it = channels_.find(handle);
    if (it == channels_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "relay channel not found");
    }

    if (it->second.state == RelayChannelState::Closed || it->second.state == RelayChannelState::Reset) {
        return core::make_error(core::ErrorCode::InvalidState, "relay channel is not active");
    }

    if (from == RelayEndpointKind::Source) {
        ++it->second.stats.frames_from_source;
        it->second.stats.bytes_from_source += payload_bytes;
    } else {
        ++it->second.stats.frames_from_target;
        it->second.stats.bytes_from_target += payload_bytes;
    }

    return core::success();
}

core::Result<void> RelayChannelManager::set_backpressured(ChannelHandle handle, bool backpressured) {
    auto it = channels_.find(handle);
    if (it == channels_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "relay channel not found");
    }

    if (it->second.state == RelayChannelState::Closed || it->second.state == RelayChannelState::Reset) {
        return core::make_error(core::ErrorCode::InvalidState, "relay channel is not active");
    }

    if (backpressured && it->second.state != RelayChannelState::Backpressured) {
        ++it->second.stats.backpressure_events;
        it->second.state = RelayChannelState::Backpressured;
    } else if (!backpressured && it->second.state == RelayChannelState::Backpressured) {
        it->second.state = RelayChannelState::Active;
    }

    return core::success();
}

std::size_t RelayChannelManager::active_channel_count() const noexcept {
    std::size_t count = 0;
    for (const auto& item : channels_) {
        if (item.second.state == RelayChannelState::Active || item.second.state == RelayChannelState::Backpressured) {
            ++count;
        }
    }
    return count;
}

core::ConnectionRef opposite_endpoint(const RelayChannelRecord& channel, RelayEndpointKind from) {
    return from == RelayEndpointKind::Source ? channel.target_connection : channel.source_connection;
}

} 
