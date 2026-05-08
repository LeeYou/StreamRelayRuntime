#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/result.h"
#include "gateway/websocket_gateway_adapter.h"
#include "gateway/websocket_gateway_listener.h"
#include "transport/tcp_listener.h"

namespace streamrelay::gateway {

enum class GatewaySocketAcceptKind {
    Admin,
    Device,
};

struct GatewaySocketEventLoopOptions {
    transport::TcpListenerOptions tcp;
    WebSocketGatewayListenerOptions websocket;
    GatewaySocketAcceptKind accept_kind{GatewaySocketAcceptKind::Admin};
    std::size_t max_frame_bytes{1024 * 1024};
};

struct GatewaySocketEventLoopStatus {
    bool running{false};
    std::uint16_t port{0};
    std::size_t accepted_connections{0};
    std::size_t failed_accepts{0};
    std::size_t active_connections{0};
    std::size_t frames_processed{0};
    std::size_t failed_frames{0};
    std::size_t frames_sent{0};
    std::size_t failed_writes{0};
    std::size_t closed_connections{0};
    std::size_t failed_closes{0};
};

struct GatewaySocketEventLoopStepResult {
    WebSocketGatewayAcceptResult gateway;
    std::size_t active_connections{0};
};

struct GatewaySocketEventLoopBatchResult {
    std::size_t attempted{0};
    std::size_t succeeded{0};
    std::size_t failed{0};
};

class GatewaySocketEventLoop {
public:
    explicit GatewaySocketEventLoop(WebSocketGatewayAdapter& adapter);

    core::Result<void> start(const GatewaySocketEventLoopOptions& options);
    core::Result<GatewaySocketEventLoopStepResult> run_once(std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now);
    core::Result<void> pump_frame_once(transport::TransportConnectionId transport_id, std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now);
    core::Result<GatewaySocketEventLoopBatchResult> pump_active_frames_once(std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now);
    core::Result<GatewaySocketEventLoopBatchResult> pump_ready_frames_once(std::chrono::system_clock::time_point system_now, std::chrono::steady_clock::time_point steady_now);
    core::Result<void> send_websocket_binary_once(transport::TransportConnectionId transport_id, core::ByteBuffer payload);
    core::Result<GatewaySocketEventLoopBatchResult> send_websocket_binary_to_all_once(const core::ByteBuffer& payload);
    core::Result<void> close_connection_once(transport::TransportConnectionId transport_id);
    core::Result<void> stop();
    GatewaySocketEventLoopStatus status() const noexcept;

private:
    struct ActiveConnection {
        transport::TransportConnectionId transport_id{0};
        transport::TcpConnection connection;
    };

    core::Result<core::ByteBuffer> read_websocket_frame(transport::TcpConnection& connection) const;
    core::Result<core::ByteBuffer> read_exact(transport::TcpConnection& connection, std::size_t bytes) const;
    ActiveConnection* find_connection(transport::TransportConnectionId transport_id) noexcept;
    std::vector<transport::TransportConnectionId> active_transport_ids() const;

    WebSocketGatewayAdapter& adapter_;
    transport::TcpListener listener_;
    GatewaySocketEventLoopOptions options_;
    GatewaySocketEventLoopStatus status_;
    std::vector<ActiveConnection> connections_;
};

} 
