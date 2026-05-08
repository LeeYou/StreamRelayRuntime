#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "core/types.h"

namespace streamrelay::protocol {

struct Envelope {
    std::uint32_t version{1};
    core::RequestId request_id{0};
    core::TraceId trace_id{0};
    std::string source;
    std::string target;
    std::string service;
    std::string method;
    std::string session_id;
    std::string principal_id;
    std::int64_t deadline_unix_ms{0};
    std::map<std::string, std::string> labels;
    std::string payload_type;
    core::ByteBuffer payload;
};

} 
