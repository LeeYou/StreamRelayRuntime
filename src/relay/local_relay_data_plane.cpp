#include "relay/local_relay_data_plane.h"

namespace streamrelay::relay {

LocalRelayDataPlane::LocalRelayDataPlane(RelayChannelManager& channels, gateway::ConnectionManager& connections)
    : channels_(channels), connections_(connections) {}

core::Result<void> LocalRelayDataPlane::forward_payload(ChannelHandle handle, RelayEndpointKind from, core::ByteBuffer payload, std::chrono::steady_clock::time_point now) {
    auto channel_result = channels_.get_channel(handle);
    if (!channel_result.ok()) {
        return channel_result.error();
    }

    auto channel = channel_result.value();
    if (channel.state != RelayChannelState::Active && channel.state != RelayChannelState::Backpressured) {
        return core::make_error(core::ErrorCode::InvalidState, "relay channel is not forwardable");
    }

    const auto target = opposite_endpoint(channel, from);
    const auto payload_size = payload.size();
    auto write_result = connections_.enqueue_write(target, std::move(payload), now);
    if (!write_result.ok()) {
        channels_.close_channel(handle, "backpressure_hard_limit", std::chrono::system_clock::now());
        return write_result.error();
    }

    auto record_result = channels_.record_forward(handle, from, payload_size);
    if (!record_result.ok()) {
        return record_result.error();
    }

    auto target_info = connections_.find_connection(target);
    if (target_info.ok()) {
        channels_.set_backpressured(handle, target_info.value().state == gateway::ConnectionState::Backpressured);
    }

    return core::success();
}

core::Result<void> LocalRelayDataPlane::forward_frame(const RelayFrame& frame, RelayEndpointKind from, std::chrono::steady_clock::time_point now) {
    if ((frame.header.flags & RelayFrameData) == 0U) {
        return core::make_error(core::ErrorCode::ProtocolError, "only data relay frames can be forwarded by local data plane");
    }

    return forward_payload(frame.header.channel_handle, from, frame.payload, now);
}

} 
