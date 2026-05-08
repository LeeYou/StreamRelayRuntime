#include "gateway/gateway_socket_event_loop.h"

#include <limits>
#include <utility>

#include "protocol/websocket_frame.h"

namespace streamrelay::gateway {
namespace {

std::uint16_t read_u16_be(const core::ByteBuffer& bytes) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[0]) << 8U) | bytes[1]);
}

std::uint64_t read_u64_be(const core::ByteBuffer& bytes) {
    std::uint64_t value = 0;
    for (auto byte : bytes) {
        value = (value << 8U) | byte;
    }
    return value;
}

void append(core::ByteBuffer& target, const core::ByteBuffer& source) {
    target.insert(target.end(), source.begin(), source.end());
}

} 

GatewaySocketEventLoop::GatewaySocketEventLoop(WebSocketGatewayAdapter& adapter) : adapter_(adapter) {}

core::Result<void> GatewaySocketEventLoop::start(const GatewaySocketEventLoopOptions& options) {
    if (status_.running) {
        return core::make_error(core::ErrorCode::InvalidState, "gateway socket event loop is already running");
    }
    if (options.max_frame_bytes == 0) {
        return core::make_error(core::ErrorCode::InvalidArgument, "gateway socket event loop max_frame_bytes must be positive");
    }

    auto started = listener_.start(options.tcp);
    if (!started.ok()) {
        return started;
    }

    options_ = options;
    const auto listener_status = listener_.status();
    status_.running = true;
    status_.port = listener_status.port;
    status_.accepted_connections = 0;
    status_.failed_accepts = 0;
    status_.active_connections = 0;
    status_.frames_processed = 0;
    status_.failed_frames = 0;
    status_.frames_sent = 0;
    status_.failed_writes = 0;
    status_.closed_connections = 0;
    status_.failed_closes = 0;
    connections_.clear();
    return core::success();
}

core::Result<GatewaySocketEventLoopStepResult> GatewaySocketEventLoop::run_once(std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now) {
    if (!status_.running) {
        return core::make_error(core::ErrorCode::InvalidState, "gateway socket event loop is not running");
    }

    WebSocketGatewayListener gateway_listener{listener_, adapter_, options_.websocket};
    core::Result<WebSocketGatewayListenerAcceptResult> accepted = core::make_error(core::ErrorCode::InvalidState, "gateway socket accept kind is unsupported");
    if (options_.accept_kind == GatewaySocketAcceptKind::Admin) {
        accepted = gateway_listener.accept_admin_once(system_now, steady_now);
    } else if (options_.accept_kind == GatewaySocketAcceptKind::Device) {
        accepted = gateway_listener.accept_device_once(system_now, steady_now);
    }

    if (!accepted.ok()) {
        ++status_.failed_accepts;
        return accepted.error();
    }

    auto value = std::move(accepted).value();
    connections_.push_back(ActiveConnection{value.gateway.edge.transport_id, std::move(value.connection)});
    ++status_.accepted_connections;
    status_.active_connections = connections_.size();
    return GatewaySocketEventLoopStepResult{value.gateway, status_.active_connections};
}

core::Result<void> GatewaySocketEventLoop::pump_frame_once(transport::TransportConnectionId transport_id, std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now) {
    if (!status_.running) {
        return core::make_error(core::ErrorCode::InvalidState, "gateway socket event loop is not running");
    }

    auto* active = find_connection(transport_id);
    if (active == nullptr) {
        ++status_.failed_frames;
        return core::make_error(core::ErrorCode::NotFound, "gateway socket connection not found");
    }

    auto frame = read_websocket_frame(active->connection);
    if (!frame.ok()) {
        ++status_.failed_frames;
        return frame.error();
    }

    auto processed = adapter_.receive_websocket_frame(transport_id, std::move(frame).value(), system_now, steady_now);
    if (!processed.ok()) {
        ++status_.failed_frames;
        return processed;
    }

    ++status_.frames_processed;
    return core::success();
}

core::Result<GatewaySocketEventLoopBatchResult> GatewaySocketEventLoop::pump_active_frames_once(std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now) {
    if (!status_.running) {
        return core::make_error(core::ErrorCode::InvalidState, "gateway socket event loop is not running");
    }

    GatewaySocketEventLoopBatchResult result;
    const auto ids = active_transport_ids();
    for (auto id : ids) {
        ++result.attempted;
        auto pumped = pump_frame_once(id, system_now, steady_now);
        if (pumped.ok()) {
            ++result.succeeded;
        } else {
            ++result.failed;
        }
    }
    return result;
}

core::Result<GatewaySocketEventLoopBatchResult> GatewaySocketEventLoop::pump_ready_frames_once(std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now) {
    if (!status_.running) {
        return core::make_error(core::ErrorCode::InvalidState, "gateway socket event loop is not running");
    }

    GatewaySocketEventLoopBatchResult result;
    const auto ids = active_transport_ids();
    for (auto id : ids) {
        auto* active = find_connection(id);
        if (active == nullptr) {
            continue;
        }

        auto readable = active->connection.readable_now();
        if (!readable.ok()) {
            ++result.attempted;
            ++result.failed;
            ++status_.failed_frames;
            continue;
        }
        if (!readable.value()) {
            continue;
        }

        ++result.attempted;
        auto pumped = pump_frame_once(id, system_now, steady_now);
        if (pumped.ok()) {
            ++result.succeeded;
        } else {
            ++result.failed;
        }
    }
    return result;
}

core::Result<void> GatewaySocketEventLoop::send_websocket_binary_once(transport::TransportConnectionId transport_id, core::ByteBuffer payload) {
    if (!status_.running) {
        return core::make_error(core::ErrorCode::InvalidState, "gateway socket event loop is not running");
    }

    auto* active = find_connection(transport_id);
    if (active == nullptr) {
        ++status_.failed_writes;
        return core::make_error(core::ErrorCode::NotFound, "gateway socket connection not found");
    }

    protocol::WebSocketFrame frame;
    frame.masked = false;
    frame.payload = std::move(payload);
    auto encoded = protocol::encode_websocket_frame(frame);
    if (!encoded.ok()) {
        ++status_.failed_writes;
        return encoded.error();
    }

    auto written = active->connection.write_all(encoded.value());
    if (!written.ok()) {
        ++status_.failed_writes;
        return written;
    }

    ++status_.frames_sent;
    return core::success();
}

core::Result<GatewaySocketEventLoopBatchResult> GatewaySocketEventLoop::send_websocket_binary_to_all_once(const core::ByteBuffer& payload) {
    if (!status_.running) {
        return core::make_error(core::ErrorCode::InvalidState, "gateway socket event loop is not running");
    }

    GatewaySocketEventLoopBatchResult result;
    const auto ids = active_transport_ids();
    for (auto id : ids) {
        ++result.attempted;
        auto written = send_websocket_binary_once(id, payload);
        if (written.ok()) {
            ++result.succeeded;
        } else {
            ++result.failed;
        }
    }
    return result;
}

core::Result<void> GatewaySocketEventLoop::close_connection_once(transport::TransportConnectionId transport_id) {
    for (auto it = connections_.begin(); it != connections_.end(); ++it) {
        if (it->transport_id != transport_id) {
            continue;
        }

        auto closed = it->connection.close();
        if (!closed.ok()) {
            ++status_.failed_closes;
            return closed;
        }

        connections_.erase(it);
        status_.active_connections = connections_.size();
        ++status_.closed_connections;
        return core::success();
    }

    ++status_.failed_closes;
    return core::make_error(core::ErrorCode::NotFound, "gateway socket connection not found");
}

core::Result<void> GatewaySocketEventLoop::stop() {
    for (auto& connection : connections_) {
        auto closed = connection.connection.close();
        if (!closed.ok()) {
            return closed;
        }
    }
    connections_.clear();
    auto stopped = listener_.stop();
    if (!stopped.ok()) {
        return stopped;
    }
    status_.running = false;
    status_.port = 0;
    status_.active_connections = 0;
    return core::success();
}

GatewaySocketEventLoopStatus GatewaySocketEventLoop::status() const noexcept {
    return status_;
}

core::Result<core::ByteBuffer> GatewaySocketEventLoop::read_websocket_frame(transport::TcpConnection& connection) const {
    auto header = read_exact(connection, 2);
    if (!header.ok()) {
        return header.error();
    }

    core::ByteBuffer frame = header.value();
    std::uint64_t payload_len = frame[1] & 0x7fU;
    if (payload_len == 126) {
        auto extended = read_exact(connection, 2);
        if (!extended.ok()) {
            return extended.error();
        }
        payload_len = read_u16_be(extended.value());
        append(frame, extended.value());
    } else if (payload_len == 127) {
        auto extended = read_exact(connection, 8);
        if (!extended.ok()) {
            return extended.error();
        }
        payload_len = read_u64_be(extended.value());
        append(frame, extended.value());
    }

    const bool masked = (frame[1] & 0x80U) != 0;
    const std::uint64_t mask_bytes = masked ? 4U : 0U;
    if (payload_len > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) || payload_len + frame.size() + mask_bytes > options_.max_frame_bytes) {
        return core::make_error(core::ErrorCode::ResourceExhausted, "websocket frame exceeds configured limit");
    }

    if (masked) {
        auto mask = read_exact(connection, 4);
        if (!mask.ok()) {
            return mask.error();
        }
        append(frame, mask.value());
    }

    auto payload = read_exact(connection, static_cast<std::size_t>(payload_len));
    if (!payload.ok()) {
        return payload.error();
    }
    append(frame, payload.value());
    return frame;
}

core::Result<core::ByteBuffer> GatewaySocketEventLoop::read_exact(transport::TcpConnection& connection, std::size_t bytes) const {
    core::ByteBuffer out;
    while (out.size() < bytes) {
        auto chunk = connection.read_some(bytes - out.size());
        if (!chunk.ok()) {
            return chunk.error();
        }
        append(out, chunk.value());
    }
    return out;
}

GatewaySocketEventLoop::ActiveConnection* GatewaySocketEventLoop::find_connection(transport::TransportConnectionId transport_id) noexcept {
    for (auto& connection : connections_) {
        if (connection.transport_id == transport_id) {
            return &connection;
        }
    }
    return nullptr;
}

std::vector<transport::TransportConnectionId> GatewaySocketEventLoop::active_transport_ids() const {
    std::vector<transport::TransportConnectionId> ids;
    ids.reserve(connections_.size());
    for (const auto& connection : connections_) {
        ids.push_back(connection.transport_id);
    }
    return ids;
}

} 
