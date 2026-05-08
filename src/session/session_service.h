#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

#include "core/result.h"
#include "core/types.h"

namespace streamrelay::session {

enum class SessionKind {
    User,
    Device,
};

struct SessionRecord {
    std::string session_id;
    std::string principal_id;
    std::string tenant_id;
    SessionKind kind{SessionKind::User};
    std::optional<core::ConnectionRef> connection;
    std::chrono::system_clock::time_point expires_at{};
    std::unordered_map<std::string, std::string> claims;
};

struct CreateSessionRequest {
    std::string tenant_id;
    std::string principal_id;
    std::unordered_map<std::string, std::string> claims;
    std::chrono::seconds ttl{std::chrono::hours(1)};
};

class InMemorySessionService {
public:
    core::Result<std::string> create_user_session(const CreateSessionRequest& request, std::chrono::system_clock::time_point now);
    core::Result<std::string> create_device_session(const CreateSessionRequest& request, std::chrono::system_clock::time_point now);
    core::Result<void> bind_connection(const std::string& session_id, core::ConnectionRef connection, std::chrono::system_clock::time_point now);
    core::Result<SessionRecord> get_session(const std::string& session_id, std::chrono::system_clock::time_point now) const;
    core::Result<SessionRecord> find_by_connection(core::ConnectionRef connection, std::chrono::system_clock::time_point now) const;
    core::Result<void> close_session(const std::string& session_id);
    core::Result<void> close_by_connection(core::ConnectionRef connection);
    void expire(std::chrono::system_clock::time_point now);
    std::size_t size() const noexcept;

private:
    core::Result<std::string> create_session(SessionKind kind, const CreateSessionRequest& request, std::chrono::system_clock::time_point now);
    bool is_expired(const SessionRecord& record, std::chrono::system_clock::time_point now) const noexcept;
    static std::string connection_key(core::ConnectionRef connection);

    std::uint64_t next_session_id_{1};
    std::unordered_map<std::string, SessionRecord> sessions_;
    std::unordered_map<std::string, std::string> principal_index_;
    std::unordered_map<std::string, std::string> connection_index_;
};

} 
