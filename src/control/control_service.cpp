#include "control/control_service.h"

#include <sstream>
#include <utility>

namespace streamrelay::control {
namespace {

void append_string(core::ByteBuffer& out, const std::string& value) {
    out.insert(out.end(), value.begin(), value.end());
    out.push_back(0);
}

core::ByteBuffer encode_dispatch_payload(const CommandRecord& record) {
    core::ByteBuffer out;
    append_string(out, record.command_id);
    append_string(out, record.tenant_id);
    append_string(out, record.device_id);
    append_string(out, record.command_type);
    out.insert(out.end(), record.payload.begin(), record.payload.end());
    return out;
}

} 

ControlService::ControlService(InMemoryCommandStore& store, InMemoryCommandAuditLog& audit, device_registry::InMemoryDeviceRegistry& devices, messaging::InMemoryMessageBus& bus)
    : store_(store), audit_(audit), devices_(devices), bus_(bus) {}

void ControlService::allow_command_type(std::string command_type) {
    allowed_command_types_.insert(std::move(command_type));
}

void ControlService::require_approval_for_risk(std::string risk_level) {
    approval_required_risks_.insert(std::move(risk_level));
}

core::Result<SubmitCommandResponse> ControlService::submit_command(const SubmitCommandRequest& request, std::chrono::system_clock::time_point now) {
    if (request.tenant_id.empty() || request.device_id.empty() || request.operator_id.empty() || request.command_type.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "tenant_id, device_id, operator_id and command_type are required");
    }

    if (!request.idempotency_key.empty()) {
        auto existing = store_.find_by_idempotency_key(request.tenant_id, request.idempotency_key);
        if (existing.ok()) {
            return SubmitCommandResponse{existing.value().command_id, existing.value().state};
        }
    }

    CommandRecord record;
    record.command_id = next_command_id(request.tenant_id);
    record.tenant_id = request.tenant_id;
    record.device_id = request.device_id;
    record.operator_id = request.operator_id;
    record.command_type = request.command_type;
    record.risk_level = request.risk_level;
    record.created_at = now;
    record.deadline_at = now + request.timeout;
    record.idempotency_key = request.idempotency_key;
    record.payload = request.payload;

    auto create_result = store_.create(record);
    if (!create_result.ok()) {
        return create_result.error();
    }

    audit(record, "created", "command created", now);

    auto policy_result = transition(record, CommandState::PolicyChecking, "policy_checking", "policy check started", now);
    if (!policy_result.ok()) {
        return policy_result.error();
    }

    if (!is_allowed_command_type(record.command_type)) {
        record.failure_reason = "command type is not allowed";
        auto rejected = transition(record, CommandState::Rejected, "rejected", record.failure_reason, now);
        if (!rejected.ok()) {
            return rejected.error();
        }
        return SubmitCommandResponse{record.command_id, record.state};
    }

    if (approval_required_risks_.find(record.risk_level) != approval_required_risks_.end()) {
        auto waiting = transition(record, CommandState::WaitingApproval, "waiting_approval", "approval required", now);
        if (!waiting.ok()) {
            return waiting.error();
        }
        return SubmitCommandResponse{record.command_id, record.state};
    }

    auto dispatching = transition(record, CommandState::Dispatching, "dispatching", "ready to dispatch", now);
    if (!dispatching.ok()) {
        return dispatching.error();
    }

    return SubmitCommandResponse{record.command_id, record.state};
}

core::Result<void> ControlService::approve_command(const std::string& command_id, std::chrono::system_clock::time_point now) {
    auto command = store_.get(command_id);
    if (!command.ok()) {
        return command.error();
    }

    auto record = command.value();
    return transition(record, CommandState::Dispatching, "approved", "approval granted", now);
}

core::Result<void> ControlService::dispatch_command(const std::string& command_id, std::chrono::system_clock::time_point now) {
    auto command = store_.get(command_id);
    if (!command.ok()) {
        return command.error();
    }

    auto record = command.value();
    if (record.deadline_at <= now) {
        record.failure_reason = "command deadline exceeded before dispatch";
        return transition(record, CommandState::Expired, "expired", record.failure_reason, now);
    }

    auto device = devices_.find_device(record.tenant_id, record.device_id, now);
    if (!device.ok() || device.value().presence.state != device_registry::DevicePresenceState::Online) {
        record.failure_reason = "target device is offline";
        return transition(record, CommandState::Failed, "dispatch_failed", record.failure_reason, now);
    }

    messaging::Message message;
    message.service = "device";
    message.method = "execute_command";
    message.payload = encode_dispatch_payload(record);
    auto publish_result = bus_.publish(message);
    if (!publish_result.ok()) {
        record.failure_reason = publish_result.error().message;
        return transition(record, CommandState::Failed, "dispatch_failed", record.failure_reason, now);
    }

    return transition(record, CommandState::Delivered, "delivered", "command delivered to device route", now);
}

core::Result<void> ControlService::handle_ack(const CommandAck& ack, std::chrono::system_clock::time_point now) {
    auto command = store_.get(ack.command_id);
    if (!command.ok()) {
        return command.error();
    }

    auto record = command.value();
    auto match = ensure_command_matches_device(record, ack.tenant_id, ack.device_id);
    if (!match.ok()) {
        return match;
    }

    if (!ack.accepted) {
        record.failure_reason = ack.reason.empty() ? "device rejected command" : ack.reason;
        return transition(record, CommandState::Rejected, "device_rejected", record.failure_reason, now);
    }

    auto accepted = transition(record, CommandState::Accepted, "accepted", "device accepted command", now);
    if (!accepted.ok()) {
        return accepted;
    }

    auto refreshed = store_.get(ack.command_id);
    if (!refreshed.ok()) {
        return refreshed.error();
    }
    auto running = refreshed.value();
    return transition(running, CommandState::Running, "running", "device started command", now);
}

core::Result<void> ControlService::append_streaming_output(const std::string& command_id, const std::string& tenant_id, const std::string& device_id, core::ByteBuffer output, std::chrono::system_clock::time_point now) {
    auto command = store_.get(command_id);
    if (!command.ok()) {
        return command.error();
    }

    auto record = command.value();
    auto match = ensure_command_matches_device(record, tenant_id, device_id);
    if (!match.ok()) {
        return match;
    }

    if (record.state != CommandState::Running && record.state != CommandState::StreamingOutput) {
        return core::make_error(core::ErrorCode::InvalidState, "streaming output is only allowed for running command");
    }

    record.streaming_outputs.push_back(std::move(output));
    auto streamed = transition(record, CommandState::StreamingOutput, "streaming_output", "streaming output appended", now);
    if (!streamed.ok()) {
        return streamed;
    }

    auto refreshed = store_.get(command_id);
    if (!refreshed.ok()) {
        return refreshed.error();
    }
    auto running = refreshed.value();
    return transition(running, CommandState::Running, "running", "command continues running", now);
}

core::Result<void> ControlService::handle_result(const CommandResult& result, std::chrono::system_clock::time_point now) {
    auto command = store_.get(result.command_id);
    if (!command.ok()) {
        return command.error();
    }

    auto record = command.value();
    auto match = ensure_command_matches_device(record, result.tenant_id, result.device_id);
    if (!match.ok()) {
        return match;
    }

    record.exit_code = result.exit_code;
    record.output = result.output;
    record.failure_reason = result.reason;
    return transition(record, result.success ? CommandState::Completed : CommandState::Failed, result.success ? "completed" : "failed", result.reason, now);
}

core::Result<void> ControlService::cancel_command(const std::string& command_id, const std::string& reason, std::chrono::system_clock::time_point now) {
    auto command = store_.get(command_id);
    if (!command.ok()) {
        return command.error();
    }

    auto record = command.value();
    record.failure_reason = reason;
    return transition(record, CommandState::Cancelled, "cancelled", reason, now);
}

std::size_t ControlService::expire_commands(std::chrono::system_clock::time_point now) {
    auto expired = store_.list_deadline_expired(now);
    for (auto& record : expired) {
        record.failure_reason = "command deadline exceeded";
        transition(record, CommandState::Expired, "expired", record.failure_reason, now);
    }
    return expired.size();
}

core::Result<void> ControlService::transition(CommandRecord& record, CommandState next, const std::string& event_type, const std::string& detail, std::chrono::system_clock::time_point now) {
    if (!can_transition(record.state, next)) {
        return core::make_error(core::ErrorCode::InvalidState, "invalid command state transition");
    }

    record.state = next;
    auto update_result = store_.update(record);
    if (!update_result.ok()) {
        return update_result;
    }

    audit(record, event_type, detail, now);
    return core::success();
}

core::Result<void> ControlService::ensure_command_matches_device(const CommandRecord& record, const std::string& tenant_id, const std::string& device_id) const {
    if (record.tenant_id != tenant_id || record.device_id != device_id) {
        return core::make_error(core::ErrorCode::InvalidArgument, "command does not match tenant/device");
    }
    return core::success();
}

bool ControlService::is_terminal(CommandState state) const noexcept {
    return state == CommandState::Completed || state == CommandState::Failed || state == CommandState::Rejected || state == CommandState::Cancelled || state == CommandState::Expired;
}

bool ControlService::can_transition(CommandState from, CommandState to) const noexcept {
    if (from == to) {
        return true;
    }

    if (is_terminal(from)) {
        return false;
    }

    switch (from) {
    case CommandState::Created:
        return to == CommandState::PolicyChecking || to == CommandState::Cancelled || to == CommandState::Expired;
    case CommandState::PolicyChecking:
        return to == CommandState::Rejected || to == CommandState::WaitingApproval || to == CommandState::Dispatching || to == CommandState::Expired;
    case CommandState::WaitingApproval:
        return to == CommandState::Dispatching || to == CommandState::Rejected || to == CommandState::Cancelled || to == CommandState::Expired;
    case CommandState::Dispatching:
        return to == CommandState::Delivered || to == CommandState::Failed || to == CommandState::Expired || to == CommandState::Cancelled;
    case CommandState::Delivered:
        return to == CommandState::Accepted || to == CommandState::Rejected || to == CommandState::Expired || to == CommandState::Cancelled;
    case CommandState::Accepted:
        return to == CommandState::Running || to == CommandState::Failed || to == CommandState::Expired || to == CommandState::Cancelled;
    case CommandState::Running:
        return to == CommandState::StreamingOutput || to == CommandState::Completed || to == CommandState::Failed || to == CommandState::Expired || to == CommandState::Cancelled;
    case CommandState::StreamingOutput:
        return to == CommandState::Running || to == CommandState::Completed || to == CommandState::Failed || to == CommandState::Expired || to == CommandState::Cancelled;
    default:
        return false;
    }
}

bool ControlService::is_allowed_command_type(const std::string& command_type) const {
    return allowed_command_types_.empty() || allowed_command_types_.find(command_type) != allowed_command_types_.end();
}

void ControlService::audit(const CommandRecord& record, const std::string& event_type, const std::string& detail, std::chrono::system_clock::time_point now) {
    CommandAuditEvent event;
    event.command_id = record.command_id;
    event.tenant_id = record.tenant_id;
    event.event_type = event_type;
    event.detail = detail;
    event.occurred_at = now;
    audit_.append(std::move(event));
}

std::string ControlService::next_command_id(const std::string& tenant_id) {
    std::ostringstream out;
    out << tenant_id << ":cmd:" << next_id_++;
    return out.str();
}

} 
