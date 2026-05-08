#include "runtime/module_manager.h"

#include <algorithm>

namespace streamrelay::runtime {

core::Result<void> ModuleManager::add_module(std::unique_ptr<IModule> module) {
    if (!module) {
        return core::make_error(core::ErrorCode::InvalidArgument, "module is null");
    }

    const auto duplicate = std::any_of(modules_.begin(), modules_.end(), [&](const auto& existing) {
        return existing->name() == module->name();
    });

    if (duplicate) {
        return core::make_error(core::ErrorCode::AlreadyExists, "module already exists: " + module->name());
    }

    modules_.push_back(std::move(module));
    return core::success();
}

core::Result<void> ModuleManager::initialize_all(AppContext& context) {
    if (initialized_) {
        return core::make_error(core::ErrorCode::InvalidState, "modules already initialized");
    }

    for (const auto& module : modules_) {
        auto result = module->initialize(context);
        if (!result.ok()) {
            return result;
        }
    }

    initialized_ = true;
    return core::success();
}

core::Result<void> ModuleManager::start_all() {
    if (!initialized_) {
        return core::make_error(core::ErrorCode::InvalidState, "modules are not initialized");
    }

    if (started_) {
        return core::make_error(core::ErrorCode::InvalidState, "modules already started");
    }

    for (const auto& module : modules_) {
        auto result = module->start();
        if (!result.ok()) {
            return result;
        }
    }

    started_ = true;
    return core::success();
}

core::Result<void> ModuleManager::stop_all() {
    if (!started_) {
        return core::success();
    }

    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
        auto result = (*it)->stop();
        if (!result.ok()) {
            return result;
        }
    }

    started_ = false;
    return core::success();
}

std::size_t ModuleManager::size() const noexcept {
    return modules_.size();
}

bool ModuleManager::initialized() const noexcept {
    return initialized_;
}

bool ModuleManager::started() const noexcept {
    return started_;
}

} // namespace streamrelay::runtime
