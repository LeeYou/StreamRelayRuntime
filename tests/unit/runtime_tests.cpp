#include <cassert>
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "core/result.h"
#include "core/types.h"
#include "control/control_service.h"
#include "device_registry/device_registry.h"
#include "gateway/connection_manager.h"
#include "gateway/gateway_runtime.h"
#include "gateway/write_queue.h"
#include "messaging/in_memory_message_bus.h"
#include "protocol/binary_codec.h"
#include "protocol/websocket_frame.h"
#include "relay/gateway_tunnel.h"
#include "relay/local_relay_data_plane.h"
#include "relay/relay_channel.h"
#include "relay/relay_frame.h"
#include "router/router.h"
#include "router/service_registry.h"
#include "runtime/app_context.h"
#include "runtime/module.h"
#include "runtime/module_manager.h"
#include "session/session_service.h"

namespace {

class RecordingModule final : public streamrelay::runtime::IModule {
public:
    explicit RecordingModule(std::string module_name, std::string& events)
        : module_name_(std::move(module_name)), events_(events) {}

    std::string name() const override {
        return module_name_;
    }

    streamrelay::core::Result<void> initialize(streamrelay::runtime::AppContext&) override {
        events_ += "init:" + module_name_ + ";";
        initialized_ = true;
        return streamrelay::core::success();
    }

    streamrelay::core::Result<void> start() override {
        assert(initialized_);
        events_ += "start:" + module_name_ + ";";
        started_ = true;
        return streamrelay::core::success();
    }

    streamrelay::core::Result<void> stop() override {
        assert(started_);
        events_ += "stop:" + module_name_ + ";";
        started_ = false;
        return streamrelay::core::success();
    }

private:
    std::string module_name_;
    std::string& events_;
    bool initialized_{false};
    bool started_{false};
};

void test_connection_ref_equality() {
    using streamrelay::core::ConnectionRef;

    const ConnectionRef first{"gateway-a", 10, 1};
    const ConnectionRef same{"gateway-a", 10, 1};
    const ConnectionRef newer{"gateway-a", 10, 2};

    assert(first == same);
    assert(first != newer);
}

void test_module_lifecycle_order() {
    std::ostringstream logs;
    streamrelay::runtime::AppContext context{"unit-test", logs};
    streamrelay::runtime::ModuleManager manager;
    std::string events;

    assert(manager.add_module(std::make_unique<RecordingModule>("first", events)).ok());
    assert(manager.add_module(std::make_unique<RecordingModule>("second", events)).ok());
    assert(manager.size() == 2);

    assert(manager.initialize_all(context).ok());
    assert(manager.initialized());
    assert(manager.start_all().ok());
    assert(manager.started());
    assert(manager.stop_all().ok());
    assert(!manager.started());

    assert(events == "init:first;init:second;start:first;start:second;stop:second;stop:first;");
}

void test_duplicate_module_rejected() {
    std::ostringstream logs;
    streamrelay::runtime::AppContext context{"unit-test", logs};
    streamrelay::runtime::ModuleManager manager;
    std::string events;

    assert(manager.add_module(std::make_unique<RecordingModule>("dup", events)).ok());
    const auto result = manager.add_module(std::make_unique<RecordingModule>("dup", events));
    assert(!result.ok());
    assert(result.error().code == streamrelay::core::ErrorCode::AlreadyExists);
}

void test_in_memory_message_bus() {
    streamrelay::messaging::InMemoryMessageBus bus;
    int handled = 0;

    const auto route = streamrelay::messaging::make_route("control", "submit_command");
    assert(bus.subscribe(route, [&](const streamrelay::messaging::Message& message) {
        assert(message.service == "control");
        assert(message.method == "submit_command");
        ++handled;
        return streamrelay::core::success();
    }).ok());

    streamrelay::messaging::Message message;
    message.request_id = 1;
    message.trace_id = 2;
    message.service = "control";
    message.method = "submit_command";

    assert(bus.publish(message).ok());
    assert(handled == 1);
    assert(bus.subscription_count() == 1);
}

void test_missing_route_rejected() {
    streamrelay::messaging::InMemoryMessageBus bus;
    streamrelay::messaging::Message message;
    message.service = "missing";
    message.method = "route";

    const auto result = bus.publish(message);
    assert(!result.ok());
    assert(result.error().code == streamrelay::core::ErrorCode::NotFound);
}

void test_envelope_frame_round_trip() {
    streamrelay::protocol::Envelope envelope;
    envelope.request_id = 100;
    envelope.trace_id = 200;
    envelope.source = "admin";
    envelope.target = "control";
    envelope.service = "control";
    envelope.method = "submit_command";
    envelope.session_id = "session-1";
    envelope.principal_id = "user-1";
    envelope.deadline_unix_ms = streamrelay::protocol::to_unix_ms(std::chrono::system_clock::now() + std::chrono::seconds(60));
    envelope.labels.emplace("tenant", "tenant-1");
    envelope.payload_type = "application/octet-stream";
    envelope.payload = {1, 2, 3, 4};

    const auto encoded = streamrelay::protocol::encode_envelope_frame(envelope);
    assert(encoded.ok());

    const auto decoded = streamrelay::protocol::decode_envelope_frame(
        encoded.value(),
        streamrelay::protocol::FrameLimits{},
        streamrelay::protocol::to_unix_ms(std::chrono::system_clock::now()));
    assert(decoded.ok());
    assert(decoded.value().request_id == envelope.request_id);
    assert(decoded.value().trace_id == envelope.trace_id);
    assert(decoded.value().service == envelope.service);
    assert(decoded.value().method == envelope.method);
    assert(decoded.value().labels.at("tenant") == "tenant-1");
    assert(decoded.value().payload == envelope.payload);
}

void test_envelope_frame_rejects_invalid_magic() {
    streamrelay::protocol::Envelope envelope;
    envelope.service = "control";
    envelope.method = "submit_command";

    auto encoded = streamrelay::protocol::encode_envelope_frame(envelope);
    assert(encoded.ok());
    auto bytes = encoded.value();
    bytes[0] = 0;

    const auto decoded = streamrelay::protocol::decode_envelope_frame(
        bytes,
        streamrelay::protocol::FrameLimits{},
        streamrelay::protocol::to_unix_ms(std::chrono::system_clock::now()));
    assert(!decoded.ok());
    assert(decoded.error().code == streamrelay::core::ErrorCode::ProtocolError);
}

void test_envelope_frame_rejects_deadline() {
    streamrelay::protocol::Envelope envelope;
    envelope.service = "control";
    envelope.method = "submit_command";
    envelope.deadline_unix_ms = streamrelay::protocol::to_unix_ms(std::chrono::system_clock::now() - std::chrono::seconds(1));

    auto encoded = streamrelay::protocol::encode_envelope_frame(envelope);
    assert(encoded.ok());

    const auto decoded = streamrelay::protocol::decode_envelope_frame(
        encoded.value(),
        streamrelay::protocol::FrameLimits{},
        streamrelay::protocol::to_unix_ms(std::chrono::system_clock::now()));
    assert(!decoded.ok());
    assert(decoded.error().code == streamrelay::core::ErrorCode::DeadlineExceeded);
}

void test_envelope_frame_rejects_size_limit() {
    streamrelay::protocol::Envelope envelope;
    envelope.service = "control";
    envelope.method = "submit_command";
    envelope.payload = std::vector<std::uint8_t>(16, 7);

    auto encoded = streamrelay::protocol::encode_envelope_frame(envelope);
    assert(encoded.ok());

    streamrelay::protocol::FrameLimits limits;
    limits.max_payload_bytes = 8;

    const auto decoded = streamrelay::protocol::decode_envelope_frame(
        encoded.value(),
        limits,
        streamrelay::protocol::to_unix_ms(std::chrono::system_clock::now()));
    assert(!decoded.ok());
    assert(decoded.error().code == streamrelay::core::ErrorCode::ResourceExhausted);
}

void test_websocket_frame_round_trip_masked_binary() {
    streamrelay::protocol::WebSocketFrame frame;
    frame.opcode = streamrelay::protocol::WebSocketOpcode::Binary;
    frame.masked = true;
    frame.masking_key = 0x11223344;
    frame.payload = streamrelay::core::ByteBuffer{1, 2, 3, 4, 5};

    auto encoded = streamrelay::protocol::encode_websocket_frame(frame);
    assert(encoded.ok());

    const auto decoded = streamrelay::protocol::decode_websocket_frame(encoded.value(), streamrelay::protocol::WebSocketFrameLimits{});
    assert(decoded.ok());
    assert(decoded.value().fin);
    assert(decoded.value().opcode == streamrelay::protocol::WebSocketOpcode::Binary);
    assert(decoded.value().masked);
    assert(decoded.value().payload == frame.payload);
}

void test_websocket_frame_rejects_unmasked_client_frame() {
    streamrelay::protocol::WebSocketFrame frame;
    frame.masked = false;
    frame.payload = streamrelay::core::ByteBuffer{1};

    auto encoded = streamrelay::protocol::encode_websocket_frame(frame);
    assert(encoded.ok());

    const auto decoded = streamrelay::protocol::decode_websocket_frame(encoded.value(), streamrelay::protocol::WebSocketFrameLimits{});
    assert(!decoded.ok());
    assert(decoded.error().code == streamrelay::core::ErrorCode::ProtocolError);
}

void test_websocket_frame_rejects_payload_limit() {
    streamrelay::protocol::WebSocketFrame frame;
    frame.masked = true;
    frame.masking_key = 0x01020304;
    frame.payload = streamrelay::core::ByteBuffer{1, 2, 3};

    auto encoded = streamrelay::protocol::encode_websocket_frame(frame);
    assert(encoded.ok());

    streamrelay::protocol::WebSocketFrameLimits limits;
    limits.max_payload_bytes = 2;
    const auto decoded = streamrelay::protocol::decode_websocket_frame(encoded.value(), limits);
    assert(!decoded.ok());
    assert(decoded.error().code == streamrelay::core::ErrorCode::ResourceExhausted);
}

void test_write_queue_soft_and_hard_limits() {
    streamrelay::gateway::WriteQueue queue{4, 8};

    assert(queue.enqueue(streamrelay::core::ByteBuffer{1, 2, 3}).ok());
    assert(queue.state() == streamrelay::gateway::WriteQueueState::Normal);
    assert(queue.enqueue(streamrelay::core::ByteBuffer{4, 5}).ok());
    assert(queue.state() == streamrelay::gateway::WriteQueueState::SoftLimited);
    assert(!queue.enqueue(streamrelay::core::ByteBuffer{6, 7, 8, 9}).ok());
    assert(queue.state() == streamrelay::gateway::WriteQueueState::SoftLimited);

    auto popped = queue.pop_front();
    assert(popped.ok());
    assert(popped.value().size() == 3);
    assert(queue.state() == streamrelay::gateway::WriteQueueState::Normal);
}

void test_connection_manager_shards_and_generation() {
    streamrelay::gateway::ConnectionManagerOptions options;
    options.gateway_id = "gateway-a";
    options.shard_count = 2;
    options.write_soft_limit_bytes = 4;
    options.write_hard_limit_bytes = 8;
    streamrelay::gateway::ConnectionManager manager{options};

    const auto now = std::chrono::steady_clock::now();
    auto first = manager.accept_connection("websocket", "127.0.0.1:1000", now);
    auto second = manager.accept_connection("websocket", "127.0.0.1:1001", now);
    assert(first.ok());
    assert(second.ok());
    assert(first.value().connection_id != second.value().connection_id);
    assert(manager.shard_index_for(first.value()) == 0);
    assert(manager.shard_index_for(second.value()) == 1);

    assert(manager.bind_connection(first.value(), streamrelay::gateway::ClientKind::Admin).ok());
    auto info = manager.find_connection(first.value());
    assert(info.ok());
    assert(info.value().state == streamrelay::gateway::ConnectionState::Active);

    auto stale = first.value();
    stale.generation += 1;
    const auto stale_result = manager.find_connection(stale);
    assert(!stale_result.ok());
    assert(stale_result.error().code == streamrelay::core::ErrorCode::InvalidState);
}

void test_connection_manager_write_backpressure() {
    streamrelay::gateway::ConnectionManagerOptions options;
    options.gateway_id = "gateway-a";
    options.shard_count = 1;
    options.write_soft_limit_bytes = 4;
    options.write_hard_limit_bytes = 8;
    streamrelay::gateway::ConnectionManager manager{options};

    const auto now = std::chrono::steady_clock::now();
    auto ref = manager.accept_connection("websocket", "127.0.0.1:1000", now);
    assert(ref.ok());
    assert(manager.bind_connection(ref.value(), streamrelay::gateway::ClientKind::DeviceAgent).ok());
    assert(manager.enqueue_write(ref.value(), streamrelay::core::ByteBuffer{1, 2, 3, 4, 5}, now).ok());

    auto info = manager.find_connection(ref.value());
    assert(info.ok());
    assert(info.value().state == streamrelay::gateway::ConnectionState::Backpressured);
    assert(info.value().write_queue_bytes == 5);

    assert(!manager.enqueue_write(ref.value(), streamrelay::core::ByteBuffer{6, 7, 8, 9}, now).ok());
    info = manager.find_connection(ref.value());
    assert(info.ok());
    assert(info.value().state == streamrelay::gateway::ConnectionState::Closing);
}

void test_connection_manager_heartbeat_timeout() {
    streamrelay::gateway::ConnectionManagerOptions options;
    options.gateway_id = "gateway-a";
    options.shard_count = 1;
    options.heartbeat_timeout = std::chrono::milliseconds{100};
    streamrelay::gateway::ConnectionManager manager{options};

    const auto now = std::chrono::steady_clock::now();
    auto ref = manager.accept_connection("websocket", "127.0.0.1:1000", now);
    assert(ref.ok());
    assert(manager.bind_connection(ref.value(), streamrelay::gateway::ClientKind::DeviceAgent).ok());

    manager.close_idle(now + std::chrono::milliseconds{101});
    const auto info = manager.find_connection(ref.value());
    assert(info.ok());
    assert(info.value().state == streamrelay::gateway::ConnectionState::Closed);
    assert(manager.active_connection_count() == 0);
}

void test_gateway_runtime_accepts_and_dispatches_valid_frame() {
    streamrelay::messaging::InMemoryMessageBus bus;
    int handled = 0;
    assert(bus.subscribe(streamrelay::messaging::make_route("control", "submit_command"), [&](const streamrelay::messaging::Message& message) {
        assert(message.request_id == 42);
        assert(message.trace_id == 84);
        assert(message.payload == streamrelay::core::ByteBuffer({9, 8, 7}));
        ++handled;
        return streamrelay::core::success();
    }).ok());

    streamrelay::gateway::GatewayRuntimeOptions options;
    options.connection_manager.gateway_id = "gateway-a";
    options.connection_manager.shard_count = 2;
    streamrelay::gateway::GatewayRuntime gateway{options, bus};

    const auto now = std::chrono::steady_clock::now();
    auto ref = gateway.accept_admin("websocket", "127.0.0.1:1000", now);
    assert(ref.ok());

    streamrelay::protocol::Envelope envelope;
    envelope.request_id = 42;
    envelope.trace_id = 84;
    envelope.service = "control";
    envelope.method = "submit_command";
    envelope.deadline_unix_ms = streamrelay::protocol::to_unix_ms(std::chrono::system_clock::now() + std::chrono::seconds(10));
    envelope.payload = streamrelay::core::ByteBuffer{9, 8, 7};

    auto frame = streamrelay::protocol::encode_envelope_frame(envelope);
    assert(frame.ok());
    assert(gateway.receive_frame(ref.value(), frame.value(), streamrelay::protocol::to_unix_ms(std::chrono::system_clock::now()), now).ok());
    assert(handled == 1);
}

void test_gateway_runtime_rejects_invalid_frame() {
    streamrelay::messaging::InMemoryMessageBus bus;
    streamrelay::gateway::GatewayRuntimeOptions options;
    options.connection_manager.gateway_id = "gateway-a";
    streamrelay::gateway::GatewayRuntime gateway{options, bus};

    const auto now = std::chrono::steady_clock::now();
    auto ref = gateway.accept_device_agent("websocket", "127.0.0.1:1000", now);
    assert(ref.ok());

    const auto result = gateway.receive_frame(ref.value(), streamrelay::core::ByteBuffer{1, 2, 3}, streamrelay::protocol::to_unix_ms(std::chrono::system_clock::now()), now);
    assert(!result.ok());
    assert(result.error().code == streamrelay::core::ErrorCode::ProtocolError);
}

void test_session_create_bind_lookup_and_expire() {
    streamrelay::session::InMemorySessionService sessions;
    const auto now = std::chrono::system_clock::now();

    streamrelay::session::CreateSessionRequest request;
    request.tenant_id = "tenant-1";
    request.principal_id = "user-1";
    request.ttl = std::chrono::seconds{10};
    request.claims.emplace("role", "admin");

    auto session_id = sessions.create_user_session(request, now);
    assert(session_id.ok());
    assert(sessions.size() == 1);

    streamrelay::core::ConnectionRef connection{"gateway-a", 11, 1};
    assert(sessions.bind_connection(session_id.value(), connection, now).ok());

    auto found = sessions.find_by_connection(connection, now);
    assert(found.ok());
    assert(found.value().session_id == session_id.value());
    assert(found.value().claims.at("role") == "admin");

    auto expired = sessions.get_session(session_id.value(), now + std::chrono::seconds{11});
    assert(!expired.ok());
    assert(expired.error().code == streamrelay::core::ErrorCode::DeadlineExceeded);

    sessions.expire(now + std::chrono::seconds{11});
    assert(sessions.size() == 0);
}

void test_session_rebind_rejects_stale_connection_close() {
    streamrelay::session::InMemorySessionService sessions;
    const auto now = std::chrono::system_clock::now();

    streamrelay::session::CreateSessionRequest request;
    request.tenant_id = "tenant-1";
    request.principal_id = "device-1";
    request.ttl = std::chrono::seconds{30};

    auto session_id = sessions.create_device_session(request, now);
    assert(session_id.ok());

    streamrelay::core::ConnectionRef old_connection{"gateway-a", 100, 1};
    streamrelay::core::ConnectionRef new_connection{"gateway-a", 100, 2};
    assert(sessions.bind_connection(session_id.value(), old_connection, now).ok());
    assert(sessions.bind_connection(session_id.value(), new_connection, now).ok());

    const auto old_close = sessions.close_by_connection(old_connection);
    assert(!old_close.ok());
    assert(old_close.error().code == streamrelay::core::ErrorCode::NotFound);

    auto found = sessions.find_by_connection(new_connection, now);
    assert(found.ok());
    assert(found.value().session_id == session_id.value());
}

void test_device_registry_register_heartbeat_lookup_and_ttl() {
    streamrelay::device_registry::InMemoryDeviceRegistry registry;
    const auto now = std::chrono::system_clock::now();

    streamrelay::device_registry::DeviceMetadata metadata;
    metadata.tenant_id = "tenant-1";
    metadata.device_id = "device-1";
    metadata.owner_user_id = "user-1";
    metadata.capabilities = {"relay", "remote_control"};
    assert(registry.register_device(metadata).ok());

    streamrelay::device_registry::DeviceHeartbeat heartbeat;
    heartbeat.tenant_id = "tenant-1";
    heartbeat.device_id = "device-1";
    heartbeat.session_id = "session-1";
    heartbeat.connection = streamrelay::core::ConnectionRef{"gateway-a", 1, 1};
    heartbeat.ttl = std::chrono::seconds{5};
    assert(registry.update_heartbeat(heartbeat, now).ok());

    auto found = registry.find_device("tenant-1", "device-1", now);
    assert(found.ok());
    assert(found.value().presence.state == streamrelay::device_registry::DevicePresenceState::Online);
    assert(found.value().presence.connection.value() == heartbeat.connection);

    auto by_owner = registry.find_by_owner("tenant-1", "user-1", now);
    assert(by_owner.size() == 1);
    auto by_capability = registry.find_by_capability("tenant-1", "remote_control", now);
    assert(by_capability.size() == 1);

    found = registry.find_device("tenant-1", "device-1", now + std::chrono::seconds{6});
    assert(found.ok());
    assert(found.value().presence.state == streamrelay::device_registry::DevicePresenceState::Offline);

    registry.expire(now + std::chrono::seconds{6});
    found = registry.find_device("tenant-1", "device-1", now + std::chrono::seconds{6});
    assert(found.ok());
    assert(found.value().presence.state == streamrelay::device_registry::DevicePresenceState::Offline);
}

void test_device_registry_stale_disconnect_does_not_remove_new_connection() {
    streamrelay::device_registry::InMemoryDeviceRegistry registry;
    const auto now = std::chrono::system_clock::now();

    streamrelay::device_registry::DeviceMetadata metadata;
    metadata.tenant_id = "tenant-1";
    metadata.device_id = "device-1";
    metadata.owner_user_id = "user-1";
    assert(registry.register_device(metadata).ok());

    streamrelay::device_registry::DeviceHeartbeat heartbeat;
    heartbeat.tenant_id = "tenant-1";
    heartbeat.device_id = "device-1";
    heartbeat.session_id = "session-1";
    heartbeat.connection = streamrelay::core::ConnectionRef{"gateway-a", 7, 1};
    heartbeat.ttl = std::chrono::seconds{30};
    assert(registry.update_heartbeat(heartbeat, now).ok());

    const auto stale_connection = heartbeat.connection;
    heartbeat.session_id = "session-2";
    heartbeat.connection = streamrelay::core::ConnectionRef{"gateway-a", 7, 2};
    assert(registry.update_heartbeat(heartbeat, now + std::chrono::seconds{1}).ok());

    const auto stale_offline = registry.mark_offline("tenant-1", "device-1", stale_connection);
    assert(!stale_offline.ok());
    assert(stale_offline.error().code == streamrelay::core::ErrorCode::InvalidState);

    auto found = registry.find_device("tenant-1", "device-1", now + std::chrono::seconds{1});
    assert(found.ok());
    assert(found.value().presence.state == streamrelay::device_registry::DevicePresenceState::Online);
    assert(found.value().presence.connection.value() == heartbeat.connection);
}

void test_relay_frame_round_trip_and_rejects_size_limit() {
    streamrelay::relay::RelayFrame frame;
    frame.header.channel_handle = 42;
    frame.header.sequence = 7;
    frame.header.ack_sequence = 6;
    frame.header.window_credit = 1024;
    frame.payload = streamrelay::core::ByteBuffer{1, 2, 3};

    auto encoded = streamrelay::relay::encode_relay_frame(frame);
    assert(encoded.ok());

    auto decoded = streamrelay::relay::decode_relay_frame(encoded.value(), streamrelay::relay::RelayFrameLimits{});
    assert(decoded.ok());
    assert(decoded.value().header.channel_handle == 42);
    assert(decoded.value().header.sequence == 7);
    assert(decoded.value().payload == frame.payload);

    streamrelay::relay::RelayFrameLimits limits;
    limits.max_payload_bytes = 2;
    auto rejected = streamrelay::relay::decode_relay_frame(encoded.value(), limits);
    assert(!rejected.ok());
    assert(rejected.error().code == streamrelay::core::ErrorCode::ResourceExhausted);
}

void test_relay_channel_lifecycle_and_stats() {
    streamrelay::relay::RelayChannelManager channels;
    const auto now = std::chrono::system_clock::now();

    streamrelay::relay::OpenRelayChannelRequest request;
    request.tenant_id = "tenant-1";
    request.source_session_id = "session-admin";
    request.target_device_id = "device-1";
    request.source_connection = streamrelay::core::ConnectionRef{"gateway-a", 1, 1};
    request.target_connection = streamrelay::core::ConnectionRef{"gateway-a", 2, 1};

    auto opened = channels.open_channel(request, now);
    assert(opened.ok());
    assert(opened.value().state == streamrelay::relay::RelayChannelState::Active);
    assert(channels.active_channel_count() == 1);

    assert(channels.record_forward(opened.value().channel_handle, streamrelay::relay::RelayEndpointKind::Source, 5).ok());
    assert(channels.set_backpressured(opened.value().channel_handle, true).ok());

    auto current = channels.get_channel(opened.value().channel_handle);
    assert(current.ok());
    assert(current.value().state == streamrelay::relay::RelayChannelState::Backpressured);
    assert(current.value().stats.frames_from_source == 1);
    assert(current.value().stats.bytes_from_source == 5);
    assert(current.value().stats.backpressure_events == 1);

    assert(channels.close_channel(opened.value().channel_handle, "normal_close", now + std::chrono::seconds{1}).ok());
    current = channels.get_channel(opened.value().channel_handle);
    assert(current.ok());
    assert(current.value().state == streamrelay::relay::RelayChannelState::Closed);
    assert(current.value().close_reason == "normal_close");
}

void test_local_relay_data_plane_forwards_bidirectional_payloads() {
    streamrelay::gateway::ConnectionManagerOptions options;
    options.gateway_id = "gateway-a";
    options.shard_count = 1;
    options.write_soft_limit_bytes = 16;
    options.write_hard_limit_bytes = 32;
    streamrelay::gateway::ConnectionManager connections{options};
    const auto now = std::chrono::steady_clock::now();

    auto source = connections.accept_connection("websocket", "admin", now);
    auto target = connections.accept_connection("websocket", "device", now);
    assert(source.ok());
    assert(target.ok());
    assert(connections.bind_connection(source.value(), streamrelay::gateway::ClientKind::Admin).ok());
    assert(connections.bind_connection(target.value(), streamrelay::gateway::ClientKind::DeviceAgent).ok());

    streamrelay::relay::RelayChannelManager channels;
    streamrelay::relay::OpenRelayChannelRequest request;
    request.tenant_id = "tenant-1";
    request.source_session_id = "session-admin";
    request.target_device_id = "device-1";
    request.source_connection = source.value();
    request.target_connection = target.value();
    auto opened = channels.open_channel(request, std::chrono::system_clock::now());
    assert(opened.ok());

    streamrelay::relay::LocalRelayDataPlane data_plane{channels, connections};
    assert(data_plane.forward_payload(opened.value().channel_handle, streamrelay::relay::RelayEndpointKind::Source, streamrelay::core::ByteBuffer{1, 2, 3}, now).ok());
    auto target_write = connections.pop_write(target.value(), now);
    assert(target_write.ok());
    assert(target_write.value() == streamrelay::core::ByteBuffer({1, 2, 3}));

    assert(data_plane.forward_payload(opened.value().channel_handle, streamrelay::relay::RelayEndpointKind::Target, streamrelay::core::ByteBuffer{4, 5}, now).ok());
    auto source_write = connections.pop_write(source.value(), now);
    assert(source_write.ok());
    assert(source_write.value() == streamrelay::core::ByteBuffer({4, 5}));
}

void test_local_relay_data_plane_backpressure_and_hard_close() {
    streamrelay::gateway::ConnectionManagerOptions options;
    options.gateway_id = "gateway-a";
    options.shard_count = 1;
    options.write_soft_limit_bytes = 4;
    options.write_hard_limit_bytes = 8;
    streamrelay::gateway::ConnectionManager connections{options};
    const auto now = std::chrono::steady_clock::now();

    auto source = connections.accept_connection("websocket", "admin", now);
    auto target = connections.accept_connection("websocket", "device", now);
    assert(source.ok());
    assert(target.ok());
    assert(connections.bind_connection(source.value(), streamrelay::gateway::ClientKind::Admin).ok());
    assert(connections.bind_connection(target.value(), streamrelay::gateway::ClientKind::DeviceAgent).ok());

    streamrelay::relay::RelayChannelManager channels;
    streamrelay::relay::OpenRelayChannelRequest request;
    request.tenant_id = "tenant-1";
    request.source_session_id = "session-admin";
    request.target_device_id = "device-1";
    request.source_connection = source.value();
    request.target_connection = target.value();
    auto opened = channels.open_channel(request, std::chrono::system_clock::now());
    assert(opened.ok());

    streamrelay::relay::LocalRelayDataPlane data_plane{channels, connections};
    assert(data_plane.forward_payload(opened.value().channel_handle, streamrelay::relay::RelayEndpointKind::Source, streamrelay::core::ByteBuffer{1, 2, 3}, now).ok());
    assert(data_plane.forward_payload(opened.value().channel_handle, streamrelay::relay::RelayEndpointKind::Source, streamrelay::core::ByteBuffer{4, 5}, now).ok());

    auto channel = channels.get_channel(opened.value().channel_handle);
    assert(channel.ok());
    assert(channel.value().state == streamrelay::relay::RelayChannelState::Backpressured);
    assert(channel.value().stats.backpressure_events == 1);

    auto hard_result = data_plane.forward_payload(opened.value().channel_handle, streamrelay::relay::RelayEndpointKind::Source, streamrelay::core::ByteBuffer{6, 7, 8, 9}, now);
    assert(!hard_result.ok());

    channel = channels.get_channel(opened.value().channel_handle);
    assert(channel.ok());
    assert(channel.value().state == streamrelay::relay::RelayChannelState::Closed);
    assert(channel.value().close_reason == "backpressure_hard_limit");
}

void register_online_test_device(streamrelay::device_registry::InMemoryDeviceRegistry& registry, std::chrono::system_clock::time_point now) {
    streamrelay::device_registry::DeviceMetadata metadata;
    metadata.tenant_id = "tenant-1";
    metadata.device_id = "device-1";
    metadata.owner_user_id = "user-1";
    metadata.capabilities = {"remote_control"};
    assert(registry.register_device(metadata).ok());

    streamrelay::device_registry::DeviceHeartbeat heartbeat;
    heartbeat.tenant_id = "tenant-1";
    heartbeat.device_id = "device-1";
    heartbeat.session_id = "device-session";
    heartbeat.connection = streamrelay::core::ConnectionRef{"gateway-a", 77, 1};
    heartbeat.ttl = std::chrono::seconds{30};
    assert(registry.update_heartbeat(heartbeat, now).ok());
}

void test_control_service_command_success_path_and_audit() {
    streamrelay::control::InMemoryCommandStore store;
    streamrelay::control::InMemoryCommandAuditLog audit;
    streamrelay::device_registry::InMemoryDeviceRegistry registry;
    streamrelay::messaging::InMemoryMessageBus bus;
    const auto now = std::chrono::system_clock::now();
    register_online_test_device(registry, now);

    int delivered = 0;
    assert(bus.subscribe(streamrelay::messaging::make_route("device", "execute_command"), [&](const streamrelay::messaging::Message& message) {
        ++delivered;
        assert(message.service == "device");
        assert(message.method == "execute_command");
        assert(!message.payload.empty());
        return streamrelay::core::success();
    }).ok());

    streamrelay::control::ControlService control{store, audit, registry, bus};
    control.allow_command_type("shell.exec");

    streamrelay::control::SubmitCommandRequest request;
    request.tenant_id = "tenant-1";
    request.device_id = "device-1";
    request.operator_id = "operator-1";
    request.command_type = "shell.exec";
    request.payload = streamrelay::core::ByteBuffer{1, 2, 3};
    request.idempotency_key = "idem-1";

    auto submitted = control.submit_command(request, now);
    assert(submitted.ok());
    assert(submitted.value().state == streamrelay::control::CommandState::Dispatching);

    auto duplicate = control.submit_command(request, now);
    assert(duplicate.ok());
    assert(duplicate.value().command_id == submitted.value().command_id);

    assert(control.dispatch_command(submitted.value().command_id, now).ok());
    assert(delivered == 1);

    streamrelay::control::CommandAck ack;
    ack.command_id = submitted.value().command_id;
    ack.tenant_id = "tenant-1";
    ack.device_id = "device-1";
    assert(control.handle_ack(ack, now).ok());

    assert(control.append_streaming_output(submitted.value().command_id, "tenant-1", "device-1", streamrelay::core::ByteBuffer{9}, now).ok());

    streamrelay::control::CommandResult result;
    result.command_id = submitted.value().command_id;
    result.tenant_id = "tenant-1";
    result.device_id = "device-1";
    result.exit_code = 0;
    result.output = streamrelay::core::ByteBuffer{4, 5};
    result.success = true;
    assert(control.handle_result(result, now).ok());

    auto command = store.get(submitted.value().command_id);
    assert(command.ok());
    assert(command.value().state == streamrelay::control::CommandState::Completed);
    assert(command.value().streaming_outputs.size() == 1);
    assert(command.value().output == result.output);
    assert(!audit.events_for(submitted.value().command_id).empty());

    const auto cancel_terminal = control.cancel_command(submitted.value().command_id, "too late", now);
    assert(!cancel_terminal.ok());
    assert(cancel_terminal.error().code == streamrelay::core::ErrorCode::InvalidState);
}

void test_control_service_rejects_unauthorized_command_and_audits() {
    streamrelay::control::InMemoryCommandStore store;
    streamrelay::control::InMemoryCommandAuditLog audit;
    streamrelay::device_registry::InMemoryDeviceRegistry registry;
    streamrelay::messaging::InMemoryMessageBus bus;
    streamrelay::control::ControlService control{store, audit, registry, bus};
    control.allow_command_type("safe.command");

    streamrelay::control::SubmitCommandRequest request;
    request.tenant_id = "tenant-1";
    request.device_id = "device-1";
    request.operator_id = "operator-1";
    request.command_type = "dangerous.command";

    auto submitted = control.submit_command(request, std::chrono::system_clock::now());
    assert(submitted.ok());
    assert(submitted.value().state == streamrelay::control::CommandState::Rejected);

    auto command = store.get(submitted.value().command_id);
    assert(command.ok());
    assert(command.value().failure_reason == "command type is not allowed");
    assert(audit.events_for(submitted.value().command_id).size() >= 3);
}

void test_control_service_approval_and_timeout() {
    streamrelay::control::InMemoryCommandStore store;
    streamrelay::control::InMemoryCommandAuditLog audit;
    streamrelay::device_registry::InMemoryDeviceRegistry registry;
    streamrelay::messaging::InMemoryMessageBus bus;
    const auto now = std::chrono::system_clock::now();
    register_online_test_device(registry, now);

    streamrelay::control::ControlService control{store, audit, registry, bus};
    control.allow_command_type("shell.exec");
    control.require_approval_for_risk("high");

    streamrelay::control::SubmitCommandRequest request;
    request.tenant_id = "tenant-1";
    request.device_id = "device-1";
    request.operator_id = "operator-1";
    request.command_type = "shell.exec";
    request.risk_level = "high";
    request.timeout = std::chrono::milliseconds{50};

    auto submitted = control.submit_command(request, now);
    assert(submitted.ok());
    assert(submitted.value().state == streamrelay::control::CommandState::WaitingApproval);

    assert(control.approve_command(submitted.value().command_id, now).ok());
    auto command = store.get(submitted.value().command_id);
    assert(command.ok());
    assert(command.value().state == streamrelay::control::CommandState::Dispatching);

    const auto expired_count = control.expire_commands(now + std::chrono::milliseconds{51});
    assert(expired_count == 1);
    command = store.get(submitted.value().command_id);
    assert(command.ok());
    assert(command.value().state == streamrelay::control::CommandState::Expired);
}

void test_service_registry_selects_best_healthy_instance_and_generation() {
    streamrelay::router::InMemoryServiceRegistry registry;
    const auto now = std::chrono::system_clock::now();

    streamrelay::router::ServiceInstance slow;
    slow.service_name = "control";
    slow.instance_id = "control-a";
    slow.endpoint = "127.0.0.1:1001";
    slow.queue_depth = 10;
    slow.inflight_requests = 3;
    slow.p95_latency_ms = 50;
    assert(registry.register_instance(slow, now).ok());

    streamrelay::router::ServiceInstance fast;
    fast.service_name = "control";
    fast.instance_id = "control-b";
    fast.endpoint = "127.0.0.1:1002";
    fast.queue_depth = 1;
    fast.inflight_requests = 1;
    fast.p95_latency_ms = 20;
    assert(registry.register_instance(fast, now).ok());

    auto selected = registry.select_best("control", now);
    assert(selected.ok());
    assert(selected.value().instance_id == "control-b");

    assert(registry.update_health("control", "control-b", false, now).ok());
    selected = registry.select_best("control", now);
    assert(selected.ok());
    assert(selected.value().instance_id == "control-a");

    auto restarted = registry.register_instance(fast, now + std::chrono::seconds{1});
    assert(restarted.ok());
    assert(restarted.value().generation == 2);
}

void test_router_routes_service_and_online_device() {
    streamrelay::router::InMemoryServiceRegistry services;
    streamrelay::device_registry::InMemoryDeviceRegistry devices;
    const auto now = std::chrono::system_clock::now();

    streamrelay::router::ServiceInstance instance;
    instance.service_name = "control";
    instance.instance_id = "control-a";
    instance.endpoint = "127.0.0.1:1001";
    assert(services.register_instance(instance, now).ok());
    register_online_test_device(devices, now);

    streamrelay::router::Router router{services, devices};
    streamrelay::protocol::Envelope envelope;
    envelope.service = "control";
    envelope.method = "submit_command";
    auto service_route = router.route_to_service(envelope, now);
    assert(service_route.ok());
    assert(service_route.value().kind == streamrelay::router::RouteKind::ServiceInstance);
    assert(service_route.value().instance_id == "control-a");

    auto device_route = router.route_to_device("tenant-1", "device-1", now);
    assert(device_route.ok());
    assert(device_route.value().kind == streamrelay::router::RouteKind::DeviceConnection);
    assert(device_route.value().gateway_id == "gateway-a");
    assert(device_route.value().connection.connection_id == 77);

    auto expired_device = router.route_to_device("tenant-1", "device-1", now + std::chrono::seconds{31});
    assert(!expired_device.ok());
    assert(expired_device.error().code == streamrelay::core::ErrorCode::NotFound);
}

void test_gateway_tunnel_manager_forwards_relay_frames_bidirectionally() {
    streamrelay::relay::GatewayTunnelManager gateway_a;
    streamrelay::relay::GatewayTunnelManager gateway_b;
    assert(gateway_a.connect("gateway-b").ok());
    assert(gateway_b.connect("gateway-a").ok());

    streamrelay::relay::RelayFrame frame;
    frame.header.channel_handle = 99;
    frame.header.sequence = 1;
    frame.payload = streamrelay::core::ByteBuffer{1, 2, 3};
    assert(gateway_a.send_frame("gateway-b", frame).ok());

    auto outbound = gateway_a.pop_outbound_frame("gateway-b");
    assert(outbound.ok());
    assert(gateway_b.receive_frame("gateway-a", outbound.value(), streamrelay::relay::RelayFrameLimits{}).ok());

    auto inbound = gateway_b.pop_inbound_frame("gateway-a");
    assert(inbound.ok());
    assert(inbound.value().header.channel_handle == 99);
    assert(inbound.value().payload == frame.payload);

    streamrelay::relay::RelayFrame reply;
    reply.header.channel_handle = 99;
    reply.header.sequence = 2;
    reply.payload = streamrelay::core::ByteBuffer{4, 5};
    assert(gateway_b.send_frame("gateway-a", reply).ok());
    outbound = gateway_b.pop_outbound_frame("gateway-a");
    assert(outbound.ok());
    assert(gateway_a.receive_frame("gateway-b", outbound.value(), streamrelay::relay::RelayFrameLimits{}).ok());
    inbound = gateway_a.pop_inbound_frame("gateway-b");
    assert(inbound.ok());
    assert(inbound.value().payload == reply.payload);

    auto stats = gateway_a.stats("gateway-b");
    assert(stats.ok());
    assert(stats.value().frames_sent == 1);
    assert(stats.value().frames_received == 1);
}

} 

int main() {
    test_connection_ref_equality();
    test_module_lifecycle_order();
    test_duplicate_module_rejected();
    test_in_memory_message_bus();
    test_missing_route_rejected();
    test_envelope_frame_round_trip();
    test_envelope_frame_rejects_invalid_magic();
    test_envelope_frame_rejects_deadline();
    test_envelope_frame_rejects_size_limit();
    test_websocket_frame_round_trip_masked_binary();
    test_websocket_frame_rejects_unmasked_client_frame();
    test_websocket_frame_rejects_payload_limit();
    test_write_queue_soft_and_hard_limits();
    test_connection_manager_shards_and_generation();
    test_connection_manager_write_backpressure();
    test_connection_manager_heartbeat_timeout();
    test_gateway_runtime_accepts_and_dispatches_valid_frame();
    test_gateway_runtime_rejects_invalid_frame();
    test_session_create_bind_lookup_and_expire();
    test_session_rebind_rejects_stale_connection_close();
    test_device_registry_register_heartbeat_lookup_and_ttl();
    test_device_registry_stale_disconnect_does_not_remove_new_connection();
    test_relay_frame_round_trip_and_rejects_size_limit();
    test_relay_channel_lifecycle_and_stats();
    test_local_relay_data_plane_forwards_bidirectional_payloads();
    test_local_relay_data_plane_backpressure_and_hard_close();
    test_control_service_command_success_path_and_audit();
    test_control_service_rejects_unauthorized_command_and_audits();
    test_control_service_approval_and_timeout();
    test_service_registry_selects_best_healthy_instance_and_generation();
    test_router_routes_service_and_online_device();
    test_gateway_tunnel_manager_forwards_relay_frames_bidirectionally();
    return 0;
}
