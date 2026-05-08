#include "session/session_service.h"

#include <sstream>
#include <utility>
#include <vector>

namespace streamrelay::session {

core::Result<std::string> InMemorySessionService::create_user_session(const CreateSessionRequest& request, std::chrono::system_clock::time_point now) {
    return create_session(SessionKind::User, request, now);
}

core::Result<std::string> InMemorySessionService::create_device_session(const CreateSessionRequest& request, std::chrono::system_clock::time_point now) {
    return create_session(SessionKind::Device, request, now);
}

core::Result<void> InMemorySessionService::bind_connection(const std::string& session_id, core::ConnectionRef connection, std::chrono::system_clock::time_point now) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "session not found");
    }

    if (is_expired(it->second, now)) {
        return core::make_error(core::ErrorCode::DeadlineExceeded, "session expired");
    }

    if (it->second.connection.has_value()) {
        connection_index_.erase(connection_key(it->second.connection.value()));
    }

    it->second.connection = std::move(connection);
    connection_index_[connection_key(it->second.connection.value())] = session_id;
    return core::success();
}

core::Result<SessionRecord> InMemorySessionService::get_session(const std::string& session_id, std::chrono::system_clock::time_point now) const {
    const auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "session not found");
    }

    if (is_expired(it->second, now)) {
        return core::make_error(core::ErrorCode::DeadlineExceeded, "session expired");
    }

    return it->second;
}

core::Result<SessionRecord> InMemorySessionService::find_by_connection(core::ConnectionRef connection, std::chrono::system_clock::time_point now) const {
    const auto index_it = connection_index_.find(connection_key(connection));
    if (index_it == connection_index_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "connection session not found");
    }

    return get_session(index_it->second, now);
}

core::Result<void> InMemorySessionService::close_session(const std::string& session_id) {
    const auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "session not found");
    }

    if (it->second.connection.has_value()) {
        connection_index_.erase(connection_key(it->second.connection.value()));
    }

    principal_index_.erase(it->second.tenant_id + ":" + it->second.principal_id);
    sessions_.erase(it);
    return core::success();
}

core::Result<void> InMemorySessionService::close_by_connection(core::ConnectionRef connection) {
    const auto key = connection_key(connection);
    const auto index_it = connection_index_.find(key);
    if (index_it == connection_index_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "connection session not found");
    }

    auto session_it = sessions_.find(index_it->second);
    if (session_it == sessions_.end() || !session_it->second.connection.has_value() || session_it->second.connection.value() != connection) {
        connection_index_.erase(index_it);
        return core::make_error(core::ErrorCode::InvalidState, "stale connection binding");
    }

    return close_session(session_it->second.session_id);
}

void InMemorySessionService::expire(std::chrono::system_clock::time_point now) {
    std::vector<std::string> expired;
    for (const auto& item : sessions_) {
        if (is_expired(item.second, now)) {
            expired.push_back(item.first);
        }
    }

    for (const auto& session_id : expired) {
        close_session(session_id);
    }
}

std::size_t InMemorySessionService::size() const noexcept {
    return sessions_.size();
}

core::Result<std::string> InMemorySessionService::create_session(SessionKind kind, const CreateSessionRequest& request, std::chrono::system_clock::time_point now) {
    if (request.tenant_id.empty() || request.principal_id.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "tenant_id and principal_id are required");
    }

    const auto principal_key = request.tenant_id + ":" + request.principal_id;
    const auto old_it = principal_index_.find(principal_key);
    if (old_it != principal_index_.end()) {
        close_session(old_it->second);
    }

    std::ostringstream id;
    id << request.tenant_id << ":" << request.principal_id << ":" << next_session_id_++;

    SessionRecord record;
    record.session_id = id.str();
    record.principal_id = request.principal_id;
    record.tenant_id = request.tenant_id;
    record.kind = kind;
    record.expires_at = now + request.ttl;
    record.claims = request.claims;

    principal_index_[principal_key] = record.session_id;
    sessions_[record.session_id] = record;
    return record.session_id;
}

bool InMemorySessionService::is_expired(const SessionRecord& record, std::chrono::system_clock::time_point now) const noexcept {
    return record.expires_at <= now;
}

std::string InMemorySessionService::connection_key(core::ConnectionRef connection) {
    return connection.gateway_id + ":" + std::to_string(connection.connection_id) + ":" + std::to_string(connection.generation);
}

} 
