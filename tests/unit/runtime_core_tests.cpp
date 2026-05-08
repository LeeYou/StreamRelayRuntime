#include <cassert>
#include <memory>
#include <sstream>
#include <string>

#include "messaging/in_memory_message_bus.h"
#include "runtime/app_context.h"
#include "runtime/module.h"
#include "runtime/module_manager.h"

namespace {
class RecordingModule final : public streamrelay::runtime::IModule {
public:
    explicit RecordingModule(std::string name, std::string& events) : name_(std::move(name)), events_(events) {}
    std::string name() const override { return name_; }
    streamrelay::core::Result<void> initialize(streamrelay::runtime::AppContext&) override { events_ += "init:" + name_ + ";"; return streamrelay::core::success(); }
    streamrelay::core::Result<void> start() override { events_ += "start:" + name_ + ";"; return streamrelay::core::success(); }
    streamrelay::core::Result<void> stop() override { events_ += "stop:" + name_ + ";"; return streamrelay::core::success(); }
private:
    std::string name_;
    std::string& events_;
};
}

int main() {
    std::ostringstream logs;
    streamrelay::runtime::AppContext context{"unit-test", logs};
    streamrelay::runtime::ModuleManager manager;
    std::string events;
    assert(manager.add_module(std::make_unique<RecordingModule>("first", events)).ok());
    assert(manager.initialize_all(context).ok());
    assert(manager.start_all().ok());
    assert(manager.stop_all().ok());

    streamrelay::messaging::InMemoryMessageBus bus;
    int handled = 0;
    assert(bus.subscribe(streamrelay::messaging::make_route("control", "submit_command"), [&](const streamrelay::messaging::Message&) { ++handled; return streamrelay::core::success(); }).ok());
    streamrelay::messaging::Message message;
    message.service = "control";
    message.method = "submit_command";
    assert(bus.publish(message).ok());
    assert(handled == 1);
    return 0;
}
