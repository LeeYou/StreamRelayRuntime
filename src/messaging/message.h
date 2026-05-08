#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace streamrelay::messaging {

struct Message {
    std::uint64_t request_id{0};
    std::uint64_t trace_id{0};
    std::string service;
    std::string method;
    std::vector<std::uint8_t> payload;
};

} 
