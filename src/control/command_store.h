#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "control/command.h"
#include "core/result.h"

namespace streamrelay::control {

class ICommandStore {
public:
    virtual ~ICommandStore() = default;

    virtual core::Result<void> create(CommandRecord record) = 0;
    virtual core::Result<void> update(const CommandRecord& record) = 0;
    virtual core::Result<CommandRecord> get(const std::string& command_id) const = 0;
    virtual core::Result<CommandRecord> find_by_idempotency_key(const std::string& tenant_id, const std::string& idempotency_key) const = 0;
    virtual std::vector<CommandRecord> list_deadline_expired(std::chrono::system_clock::time_point now) const = 0;
    virtual std::size_t size() const noexcept = 0;
};

class ICommandAuditLog {
public:
    virtual ~ICommandAuditLog() = default;

    virtual void append(CommandAuditEvent event) = 0;
    virtual const std::vector<CommandAuditEvent>& events() const noexcept = 0;
    virtual std::vector<CommandAuditEvent> events_for(const std::string& command_id) const = 0;
};

class InMemoryCommandStore final : public ICommandStore {
public:
    core::Result<void> create(CommandRecord record) override;
    core::Result<void> update(const CommandRecord& record) override;
    core::Result<CommandRecord> get(const std::string& command_id) const override;
    core::Result<CommandRecord> find_by_idempotency_key(const std::string& tenant_id, const std::string& idempotency_key) const override;
    std::vector<CommandRecord> list_deadline_expired(std::chrono::system_clock::time_point now) const override;
    std::size_t size() const noexcept override;

private:
    static std::string idempotency_index_key(const std::string& tenant_id, const std::string& idempotency_key);

    std::unordered_map<std::string, CommandRecord> commands_;
    std::unordered_map<std::string, std::string> idempotency_index_;
};

class InMemoryCommandAuditLog final : public ICommandAuditLog {
public:
    void append(CommandAuditEvent event) override;
    const std::vector<CommandAuditEvent>& events() const noexcept override;
    std::vector<CommandAuditEvent> events_for(const std::string& command_id) const override;

private:
    std::vector<CommandAuditEvent> events_;
};

} 
