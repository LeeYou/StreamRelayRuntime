#include "messaging/in_memory_message_bus.h"

#include <utility>

namespace streamrelay::messaging {

core::Result<void> InMemoryMessageBus::subscribe(std::string route, MessageHandler handler) {
    if (route.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "route is empty");
    }

    if (!handler) {
        return core::make_error(core::ErrorCode::InvalidArgument, "handler is empty");
    }

    handlers_[std::move(route)].push_back(std::move(handler));
    return core::success();
}

core::Result<void> InMemoryMessageBus::publish(const Message& message) const {
    const auto route = make_route(message.service, message.method);
    const auto it = handlers_.find(route);
    if (it == handlers_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "route not found: " + route);
    }

    for (const auto& handler : it->second) {
        auto result = handler(message);
        if (!result.ok()) {
            return result;
        }
    }

    return core::success();
}

std::size_t InMemoryMessageBus::subscription_count() const noexcept {
    std::size_t count = 0;
    for (const auto& item : handlers_) {
        count += item.second.size();
    }
    return count;
}

std::string make_route(const std::string& service, const std::string& method) {
    return service + "." + method;
}

} 
