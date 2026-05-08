#pragma once

#include <chrono>

#include "core/result.h"
#include "gateway/connection_manager.h"
#include "relay/relay_channel.h"
#include "relay/relay_frame.h"

namespace streamrelay::relay {

class LocalRelayDataPlane {
public:
    LocalRelayDataPlane(RelayChannelManager& channels, gateway::ConnectionManager& connections);

    core::Result<void> forward_payload(ChannelHandle handle, RelayEndpointKind from, core::ByteBuffer payload, std::chrono::steady_clock::time_point now);
    core::Result<void> forward_frame(const RelayFrame& frame, RelayEndpointKind from, std::chrono::steady_clock::time_point now);

private:
    RelayChannelManager& channels_;
    gateway::ConnectionManager& connections_;
};

} 
