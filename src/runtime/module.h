#pragma once

#include <string>

#include "core/result.h"

namespace streamrelay::runtime {

class AppContext;

class IModule {
public:
    virtual ~IModule() = default;

    virtual std::string name() const = 0;
    virtual core::Result<void> initialize(AppContext& context) = 0;
    virtual core::Result<void> start() = 0;
    virtual core::Result<void> stop() = 0;
};

} // namespace streamrelay::runtime
