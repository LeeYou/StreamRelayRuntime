#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

#include "core/result.h"
#include "core/types.h"
#include "session/session_service.h"

namespace streamrelay::session {

class ISessionStore {
public:
    virtual ~ISessionStore() = default;

    virtual core::Result<void> put(SessionRecord record) = 0;
    virtual core::Result<SessionRecord> get(const std::string& session_id) const = 0;
    virtual core::Result<void> remove(const std::string& session_id) = 0;
    virtual core::Result<void> bind_connection(const std::string& session_id, core::ConnectionRef connection) = 0;
    virtual core::Result<SessionRecord> find_by_connection(core::ConnectionRef connection) const = 0;
    virtual void expire(std::chrono::system_clock::time_point now) = 0;
    virtual std::size_t size() const noexcept = 0;
};

class InMemorySessionStore final : public ISessionStore {
public:
    core::Result<void> put(SessionRecord record) override;
    core::Result<SessionRecord> get(const std::string& session_id) const override;
    core::Result<void> remove(const std::string& session_id) override;
    core::Result<void> bind_connection(const std::string& session_id, core::ConnectionRef connection) override;
    core::Result<SessionRecord> find_by_connection(core::ConnectionRef connection) const override;
    void expire(std::chrono::system_clock::time_point now) override;
    std::size_t size() const noexcept override;

private:
    static std::string connection_key(core::ConnectionRef connection);
    static bool is_expired(const SessionRecord& record, std::chrono::system_clock::time_point now) noexcept;

    std::unordered_map<std::string, SessionRecord> sessions_;
    std::unordered_map<std::string, std::string> connection_index_;
};

} 
