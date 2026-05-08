#include "session/session_store.h"

#include <utility>
#include <vector>

namespace streamrelay::session {

core::Result<void> InMemorySessionStore::put(SessionRecord record) {
    if (record.session_id.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "session_id is required");
    }
    auto existing = sessions_.find(record.session_id);
    if (existing != sessions_.end() && existing->second.connection.has_value()) {
        connection_index_.erase(connection_key(existing->second.connection.value()));
    }
    if (record.connection.has_value()) {
        connection_index_[connection_key(record.connection.value())] = record.session_id;
    }
    sessions_[record.session_id] = std::move(record);
    return core::success();
}

core::Result<SessionRecord> InMemorySessionStore::get(const std::string& session_id) const {
    const auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "session not found");
    }
    return it->second;
}

core::Result<void> InMemorySessionStore::remove(const std::string& session_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "session not found");
    }
    if (it->second.connection.has_value()) {
        connection_index_.erase(connection_key(it->second.connection.value()));
    }
    sessions_.erase(it);
    return core::success();
}

core::Result<void> InMemorySessionStore::bind_connection(const std::string& session_id, core::ConnectionRef connection) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "session not found");
    }
    if (it->second.connection.has_value()) {
        connection_index_.erase(connection_key(it->second.connection.value()));
    }
    it->second.connection = connection;
    connection_index_[connection_key(connection)] = session_id;
    return core::success();
}

core::Result<SessionRecord> InMemorySessionStore::find_by_connection(core::ConnectionRef connection) const {
    const auto index_it = connection_index_.find(connection_key(connection));
    if (index_it == connection_index_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "connection session not found");
    }
    return get(index_it->second);
}

void InMemorySessionStore::expire(std::chrono::system_clock::time_point now) {
    std::vector<std::string> expired;
    for (const auto& item : sessions_) {
        if (is_expired(item.second, now)) {
            expired.push_back(item.first);
        }
    }
    for (const auto& session_id : expired) {
        remove(session_id);
    }
}

std::size_t InMemorySessionStore::size() const noexcept {
    return sessions_.size();
}

std::string InMemorySessionStore::connection_key(core::ConnectionRef connection) {
    return connection.gateway_id + ":" + std::to_string(connection.connection_id) + ":" + std::to_string(connection.generation);
}

bool InMemorySessionStore::is_expired(const SessionRecord& record, std::chrono::system_clock::time_point now) noexcept {
    return record.expires_at <= now;
}

} 
