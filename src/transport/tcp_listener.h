#pragma once

#include <cstdint>
#include <string>

#include "core/result.h"
#include "core/types.h"

namespace streamrelay::transport {

struct TcpListenerOptions {
    std::string bind_address{"127.0.0.1"};
    std::uint16_t port{0};
    int backlog{128};
};

struct TcpListenerStatus {
    bool running{false};
    std::string bind_address;
    std::uint16_t port{0};
};

struct TcpAcceptedConnection {
    std::uintptr_t native_handle{0};
    std::string remote_address;
    std::uint16_t remote_port{0};
};

class TcpConnection {
public:
    TcpConnection() = default;
    explicit TcpConnection(TcpAcceptedConnection accepted);
    ~TcpConnection();

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    TcpConnection(TcpConnection&& other) noexcept;
    TcpConnection& operator=(TcpConnection&& other) noexcept;

    core::Result<core::ByteBuffer> read_some(std::size_t max_bytes);
    core::Result<void> write_all(const core::ByteBuffer& bytes);
    core::Result<void> close();
    bool open() const noexcept;
    const TcpAcceptedConnection& info() const noexcept;

private:
    TcpAcceptedConnection accepted_;
};

class TcpListener {
public:
    TcpListener() = default;
    ~TcpListener();

    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;

    core::Result<void> start(const TcpListenerOptions& options);
    core::Result<TcpConnection> accept_once();
    core::Result<void> stop();
    TcpListenerStatus status() const noexcept;

private:
#ifdef _WIN32
    using NativeSocket = std::uintptr_t;
#else
    using NativeSocket = int;
#endif

    NativeSocket socket_{0};
    TcpListenerStatus status_;
};

} 
