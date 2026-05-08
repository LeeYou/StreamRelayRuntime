#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/result.h"
#include "runtime/module.h"

namespace streamrelay::runtime {

class ModuleManager {
public:
    core::Result<void> add_module(std::unique_ptr<IModule> module);
    core::Result<void> initialize_all(AppContext& context);
    core::Result<void> start_all();
    core::Result<void> stop_all();

    std::size_t size() const noexcept;
    bool initialized() const noexcept;
    bool started() const noexcept;

private:
    std::vector<std::unique_ptr<IModule>> modules_;
    bool initialized_{false};
    bool started_{false};
};

} // namespace streamrelay::runtime
