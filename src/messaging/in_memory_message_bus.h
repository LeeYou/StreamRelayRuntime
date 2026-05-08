#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/result.h"
#include "messaging/message.h"

namespace streamrelay::messaging {

using MessageHandler = std::function<core::Result<void>(const Message&)>;

class InMemoryMessageBus {
public:
    core::Result<void> subscribe(std::string route, MessageHandler handler);
    core::Result<void> publish(const Message& message) const;
    std::size_t subscription_count() const noexcept;

private:
    std::unordered_map<std::string, std::vector<MessageHandler>> handlers_;
};

std::string make_route(const std::string& service, const std::string& method);

} 
