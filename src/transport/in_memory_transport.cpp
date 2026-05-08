#include "transport/in_memory_transport.h"

#include <utility>

namespace streamrelay::transport {

core::Result<TransportConnectionId> InMemoryTransportServer::accept(TransportClientKind kind, std::string remote_address) {
    TransportConnectionInfo info;
    info.id = next_id_++;
    info.kind = kind;
    info.remote_address = std::move(remote_address);
    connections_[info.id] = Slot{info, {}, {}};
    return info.id;
}

core::Result<void> InMemoryTransportServer::receive_from_client(TransportConnectionId id, core::ByteBuffer frame) {
    auto slot = find_open(id);
    if (!slot.ok()) {
        return slot.error();
    }
    slot.value()->inbound.push_back(std::move(frame));
    return core::success();
}

core::Result<core::ByteBuffer> InMemoryTransportServer::pop_received(TransportConnectionId id) {
    auto slot = find_open(id);
    if (!slot.ok()) {
        return slot.error();
    }
    if (slot.value()->inbound.empty()) {
        return core::make_error(core::ErrorCode::NotFound, "transport inbound queue is empty");
    }
    auto frame = std::move(slot.value()->inbound.front());
    slot.value()->inbound.pop_front();
    return frame;
}

core::Result<void> InMemoryTransportServer::send_to_client(TransportConnectionId id, core::ByteBuffer frame) {
    auto slot = find_open(id);
    if (!slot.ok()) {
        return slot.error();
    }
    slot.value()->outbound.push_back(std::move(frame));
    return core::success();
}

core::Result<core::ByteBuffer> InMemoryTransportServer::pop_sent(TransportConnectionId id) {
    auto slot = find_open(id);
    if (!slot.ok()) {
        return slot.error();
    }
    if (slot.value()->outbound.empty()) {
        return core::make_error(core::ErrorCode::NotFound, "transport outbound queue is empty");
    }
    auto frame = std::move(slot.value()->outbound.front());
    slot.value()->outbound.pop_front();
    return frame;
}

core::Result<void> InMemoryTransportServer::close(TransportConnectionId id) {
    auto it = connections_.find(id);
    if (it == connections_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "transport connection not found");
    }
    it->second.info.open = false;
    it->second.inbound.clear();
    it->second.outbound.clear();
    return core::success();
}

core::Result<TransportConnectionInfo> InMemoryTransportServer::connection_info(TransportConnectionId id) const {
    auto slot = find_open(id);
    if (!slot.ok()) {
        return slot.error();
    }
    return slot.value()->info;
}

std::size_t InMemoryTransportServer::connection_count() const noexcept {
    return connections_.size();
}

core::Result<InMemoryTransportServer::Slot*> InMemoryTransportServer::find_open(TransportConnectionId id) {
    auto it = connections_.find(id);
    if (it == connections_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "transport connection not found");
    }
    if (!it->second.info.open) {
        return core::make_error(core::ErrorCode::InvalidState, "transport connection is closed");
    }
    return &it->second;
}

core::Result<const InMemoryTransportServer::Slot*> InMemoryTransportServer::find_open(TransportConnectionId id) const {
    auto it = connections_.find(id);
    if (it == connections_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "transport connection not found");
    }
    if (!it->second.info.open) {
        return core::make_error(core::ErrorCode::InvalidState, "transport connection is closed");
    }
    return &it->second;
}

} 
