#include "control/command_store.h"

#include <utility>

namespace streamrelay::control {

core::Result<void> InMemoryCommandStore::create(CommandRecord record) {
    if (record.command_id.empty() || record.tenant_id.empty() || record.device_id.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "command_id, tenant_id and device_id are required");
    }

    if (commands_.find(record.command_id) != commands_.end()) {
        return core::make_error(core::ErrorCode::AlreadyExists, "command already exists");
    }

    if (!record.idempotency_key.empty()) {
        const auto key = idempotency_index_key(record.tenant_id, record.idempotency_key);
        if (idempotency_index_.find(key) != idempotency_index_.end()) {
            return core::make_error(core::ErrorCode::AlreadyExists, "idempotency key already exists");
        }
        idempotency_index_[key] = record.command_id;
    }

    commands_[record.command_id] = std::move(record);
    return core::success();
}

core::Result<void> InMemoryCommandStore::update(const CommandRecord& record) {
    auto it = commands_.find(record.command_id);
    if (it == commands_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "command not found");
    }

    it->second = record;
    return core::success();
}

core::Result<CommandRecord> InMemoryCommandStore::get(const std::string& command_id) const {
    const auto it = commands_.find(command_id);
    if (it == commands_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "command not found");
    }

    return it->second;
}

core::Result<CommandRecord> InMemoryCommandStore::find_by_idempotency_key(const std::string& tenant_id, const std::string& idempotency_key) const {
    const auto index_it = idempotency_index_.find(idempotency_index_key(tenant_id, idempotency_key));
    if (index_it == idempotency_index_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "command idempotency key not found");
    }

    return get(index_it->second);
}

std::vector<CommandRecord> InMemoryCommandStore::list_deadline_expired(std::chrono::system_clock::time_point now) const {
    std::vector<CommandRecord> out;
    for (const auto& item : commands_) {
        const auto& command = item.second;
        if (command.deadline_at <= now && command.state != CommandState::Completed && command.state != CommandState::Failed && command.state != CommandState::Rejected && command.state != CommandState::Cancelled && command.state != CommandState::Expired) {
            out.push_back(command);
        }
    }
    return out;
}

std::size_t InMemoryCommandStore::size() const noexcept {
    return commands_.size();
}

std::string InMemoryCommandStore::idempotency_index_key(const std::string& tenant_id, const std::string& idempotency_key) {
    return tenant_id + ":" + idempotency_key;
}

void InMemoryCommandAuditLog::append(CommandAuditEvent event) {
    events_.push_back(std::move(event));
}

const std::vector<CommandAuditEvent>& InMemoryCommandAuditLog::events() const noexcept {
    return events_;
}

std::vector<CommandAuditEvent> InMemoryCommandAuditLog::events_for(const std::string& command_id) const {
    std::vector<CommandAuditEvent> out;
    for (const auto& event : events_) {
        if (event.command_id == command_id) {
            out.push_back(event);
        }
    }
    return out;
}

} 
