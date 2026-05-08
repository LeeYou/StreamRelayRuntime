#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/types.h"

namespace streamrelay::control {

enum class CommandState {
    Created,
    PolicyChecking,
    Rejected,
    WaitingApproval,
    Dispatching,
    Delivered,
    Accepted,
    Running,
    StreamingOutput,
    Completed,
    Failed,
    Cancelled,
    Expired,
};

struct CommandRecord {
    std::string command_id;
    std::string tenant_id;
    std::string device_id;
    std::string operator_id;
    std::string command_type;
    std::string risk_level;
    CommandState state{CommandState::Created};
    std::uint32_t retry_count{0};
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point deadline_at{};
    std::string idempotency_key;
    core::ByteBuffer payload;
    std::int32_t exit_code{0};
    core::ByteBuffer output;
    std::vector<core::ByteBuffer> streaming_outputs;
    std::string failure_reason;
    std::unordered_map<std::string, std::string> attributes;
};

struct CommandAuditEvent {
    std::string command_id;
    std::string tenant_id;
    std::string event_type;
    std::string detail;
    std::chrono::system_clock::time_point occurred_at{};
};

} 
