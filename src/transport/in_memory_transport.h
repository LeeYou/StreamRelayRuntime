#pragma once

#include <deque>
#include <string>
#include <unordered_map>

#include "core/result.h"
#include "core/types.h"

namespace streamrelay::transport {

using TransportConnectionId = std::uint64_t;

enum class TransportClientKind {
    Admin,
    DeviceAgent,
    InternalGateway,
};

struct TransportConnectionInfo {
    TransportConnectionId id{0};
    TransportClientKind kind{TransportClientKind::Admin};
    std::string remote_address;
    bool open{true};
};

class InMemoryTransportServer {
public:
    core::Result<TransportConnectionId> accept(TransportClientKind kind, std::string remote_address);
    core::Result<void> receive_from_client(TransportConnectionId id, core::ByteBuffer frame);
    core::Result<core::ByteBuffer> pop_received(TransportConnectionId id);
    core::Result<void> send_to_client(TransportConnectionId id, core::ByteBuffer frame);
    core::Result<core::ByteBuffer> pop_sent(TransportConnectionId id);
    core::Result<void> close(TransportConnectionId id);
    core::Result<TransportConnectionInfo> connection_info(TransportConnectionId id) const;
    std::size_t connection_count() const noexcept;

private:
    struct Slot {
        TransportConnectionInfo info;
        std::deque<core::ByteBuffer> inbound;
        std::deque<core::ByteBuffer> outbound;
    };

    core::Result<Slot*> find_open(TransportConnectionId id);
    core::Result<const Slot*> find_open(TransportConnectionId id) const;

    TransportConnectionId next_id_{1};
    std::unordered_map<TransportConnectionId, Slot> connections_;
};

} 
