#include <cassert>
#include <chrono>

#include "control/command_store.h"

int main() {
    const auto now = std::chrono::system_clock::now();

    streamrelay::control::InMemoryCommandStore memory_store;
    streamrelay::control::ICommandStore& store = memory_store;

    streamrelay::control::CommandRecord command;
    command.command_id = "command-1";
    command.tenant_id = "tenant-1";
    command.device_id = "device-1";
    command.operator_id = "operator-1";
    command.command_type = "shell.exec";
    command.idempotency_key = "idem-1";
    command.created_at = now;
    command.deadline_at = now + std::chrono::seconds{5};
    command.payload = streamrelay::core::ByteBuffer{1, 2, 3};

    auto created = store.create(command);
    assert(created.ok());
    assert(store.size() == 1);

    auto loaded = store.get("command-1");
    assert(loaded.ok());
    assert(loaded.value().command_type == "shell.exec");

    auto by_idempotency = store.find_by_idempotency_key("tenant-1", "idem-1");
    assert(by_idempotency.ok());
    assert(by_idempotency.value().command_id == "command-1");

    command.state = streamrelay::control::CommandState::Dispatching;
    auto updated = store.update(command);
    assert(updated.ok());
    auto expired_before_deadline = store.list_deadline_expired(now + std::chrono::seconds{4});
    assert(expired_before_deadline.empty());
    auto expired_after_deadline = store.list_deadline_expired(now + std::chrono::seconds{6});
    assert(expired_after_deadline.size() == 1);
    assert(expired_after_deadline[0].command_id == "command-1");

    command.state = streamrelay::control::CommandState::Completed;
    updated = store.update(command);
    assert(updated.ok());
    expired_after_deadline = store.list_deadline_expired(now + std::chrono::seconds{6});
    assert(expired_after_deadline.empty());

    streamrelay::control::InMemoryCommandAuditLog memory_audit;
    streamrelay::control::ICommandAuditLog& audit = memory_audit;
    streamrelay::control::CommandAuditEvent event;
    event.command_id = "command-1";
    event.tenant_id = "tenant-1";
    event.event_type = "created";
    event.detail = "command created";
    event.occurred_at = now;
    audit.append(event);

    assert(audit.events().size() == 1);
    auto command_events = audit.events_for("command-1");
    assert(command_events.size() == 1);
    assert(command_events[0].event_type == "created");
    assert(audit.events_for("missing-command").empty());
    return 0;
}
