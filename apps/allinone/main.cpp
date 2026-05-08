#include <iostream>
#include <memory>
#include <string>

#include "core/result.h"
#include "runtime/app_context.h"
#include "runtime/module.h"
#include "runtime/module_manager.h"

namespace {

class SmokeModule final : public streamrelay::runtime::IModule {
public:
    std::string name() const override {
        return "smoke";
    }

    streamrelay::core::Result<void> initialize(streamrelay::runtime::AppContext& context) override {
        context.log_stream() << "initialize " << name() << '\n';
        initialized_ = true;
        return streamrelay::core::success();
    }

    streamrelay::core::Result<void> start() override {
        if (!initialized_) {
            return streamrelay::core::make_error(streamrelay::core::ErrorCode::InvalidState, "module is not initialized");
        }

        started_ = true;
        return streamrelay::core::success();
    }

    streamrelay::core::Result<void> stop() override {
        started_ = false;
        return streamrelay::core::success();
    }

private:
    bool initialized_{false};
    bool started_{false};
};

} 

int main() {
    streamrelay::runtime::AppContext context{"streamrelay_allinone", std::cout};
    streamrelay::runtime::ModuleManager modules;

    auto add_result = modules.add_module(std::make_unique<SmokeModule>());
    if (!add_result.ok()) {
        std::cerr << add_result.error().message << '\n';
        return 1;
    }

    auto init_result = modules.initialize_all(context);
    if (!init_result.ok()) {
        std::cerr << init_result.error().message << '\n';
        return 1;
    }

    auto start_result = modules.start_all();
    if (!start_result.ok()) {
        std::cerr << start_result.error().message << '\n';
        return 1;
    }

    auto stop_result = modules.stop_all();
    if (!stop_result.ok()) {
        std::cerr << stop_result.error().message << '\n';
        return 1;
    }

    std::cout << "streamrelay_allinone stopped" << '\n';
    return 0;
}
