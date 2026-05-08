#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "control/command.h"
#include "core/result.h"

namespace streamrelay::control {

class InMemoryCommandStore {
public:
    core::Result<void> create(CommandRecord record);
    core::Result<void> update(const CommandRecord& record);
    core::Result<CommandRecord> get(const std::string& command_id) const;
    core::Result<CommandRecord> find_by_idempotency_key(const std::string& tenant_id, const std::string& idempotency_key) const;
    std::vector<CommandRecord> list_deadline_expired(std::chrono::system_clock::time_point now) const;
    std::size_t size() const noexcept;

private:
    static std::string idempotency_index_key(const std::string& tenant_id, const std::string& idempotency_key);

    std::unordered_map<std::string, CommandRecord> commands_;
    std::unordered_map<std::string, std::string> idempotency_index_;
};

class InMemoryCommandAuditLog {
public:
    void append(CommandAuditEvent event);
    const std::vector<CommandAuditEvent>& events() const noexcept;
    std::vector<CommandAuditEvent> events_for(const std::string& command_id) const;

private:
    std::vector<CommandAuditEvent> events_;
};

} 
