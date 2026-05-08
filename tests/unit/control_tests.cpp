#include <cassert>
#include <chrono>

#include "control/control_service.h"

int main() {
    streamrelay::control::InMemoryCommandStore store;
    streamrelay::control::InMemoryCommandAuditLog audit;
    streamrelay::device_registry::InMemoryDeviceRegistry devices;
    streamrelay::messaging::InMemoryMessageBus bus;
    streamrelay::control::ControlService control{store, audit, devices, bus};
    control.allow_command_type("safe.command");

    streamrelay::control::SubmitCommandRequest request;
    request.tenant_id = "tenant-1";
    request.device_id = "device-1";
    request.operator_id = "operator-1";
    request.command_type = "dangerous.command";
    auto rejected = control.submit_command(request, std::chrono::system_clock::now());
    assert(rejected.ok());
    assert(rejected.value().state == streamrelay::control::CommandState::Rejected);
    assert(!audit.events_for(rejected.value().command_id).empty());
    return 0;
}
