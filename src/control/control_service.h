#pragma once

#include <chrono>
#include <string>
#include <unordered_set>

#include "control/command_store.h"
#include "core/result.h"
#include "core/types.h"
#include "device_registry/device_registry.h"
#include "messaging/in_memory_message_bus.h"

namespace streamrelay::control {

struct SubmitCommandRequest {
    std::string tenant_id;
    std::string device_id;
    std::string operator_id;
    std::string command_type;
    core::ByteBuffer payload;
    std::chrono::milliseconds timeout{std::chrono::seconds(30)};
    std::string idempotency_key;
    std::string risk_level{"low"};
};

struct SubmitCommandResponse {
    std::string command_id;
    CommandState state{CommandState::Created};
};

struct CommandAck {
    std::string command_id;
    std::string tenant_id;
    std::string device_id;
    bool accepted{true};
    std::string reason;
};

struct CommandResult {
    std::string command_id;
    std::string tenant_id;
    std::string device_id;
    std::int32_t exit_code{0};
    core::ByteBuffer output;
    bool success{true};
    std::string reason;
};

class ControlService {
public:
    ControlService(InMemoryCommandStore& store, InMemoryCommandAuditLog& audit, device_registry::InMemoryDeviceRegistry& devices, messaging::InMemoryMessageBus& bus);

    void allow_command_type(std::string command_type);
    void require_approval_for_risk(std::string risk_level);

    core::Result<SubmitCommandResponse> submit_command(const SubmitCommandRequest& request, std::chrono::system_clock::time_point now);
    core::Result<void> approve_command(const std::string& command_id, std::chrono::system_clock::time_point now);
    core::Result<void> dispatch_command(const std::string& command_id, std::chrono::system_clock::time_point now);
    core::Result<void> handle_ack(const CommandAck& ack, std::chrono::system_clock::time_point now);
    core::Result<void> append_streaming_output(const std::string& command_id, const std::string& tenant_id, const std::string& device_id, core::ByteBuffer output, std::chrono::system_clock::time_point now);
    core::Result<void> handle_result(const CommandResult& result, std::chrono::system_clock::time_point now);
    core::Result<void> cancel_command(const std::string& command_id, const std::string& reason, std::chrono::system_clock::time_point now);
    std::size_t expire_commands(std::chrono::system_clock::time_point now);

private:
    core::Result<void> transition(CommandRecord& record, CommandState next, const std::string& event_type, const std::string& detail, std::chrono::system_clock::time_point now);
    core::Result<void> ensure_command_matches_device(const CommandRecord& record, const std::string& tenant_id, const std::string& device_id) const;
    bool is_terminal(CommandState state) const noexcept;
    bool can_transition(CommandState from, CommandState to) const noexcept;
    bool is_allowed_command_type(const std::string& command_type) const;
    void audit(const CommandRecord& record, const std::string& event_type, const std::string& detail, std::chrono::system_clock::time_point now);
    std::string next_command_id(const std::string& tenant_id);

    InMemoryCommandStore& store_;
    InMemoryCommandAuditLog& audit_;
    device_registry::InMemoryDeviceRegistry& devices_;
    messaging::InMemoryMessageBus& bus_;
    std::uint64_t next_id_{1};
    std::unordered_set<std::string> allowed_command_types_;
    std::unordered_set<std::string> approval_required_risks_;
};

} 
