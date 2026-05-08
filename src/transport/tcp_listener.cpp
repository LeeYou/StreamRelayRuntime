#include "transport/tcp_listener.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <utility>
#endif

#include <utility>

namespace streamrelay::transport {
namespace {

core::Result<void> validate_options(const TcpListenerOptions& options) {
    if (options.bind_address.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "tcp listener bind_address is required");
    }
    if (options.backlog <= 0) {
        return core::make_error(core::ErrorCode::InvalidArgument, "tcp listener backlog must be positive");
    }
    return core::success();
}

#ifdef _WIN32

core::Error last_winsock_error(const std::string& operation) {
    return core::make_error(core::ErrorCode::InternalError, operation + " failed with WSA error " + std::to_string(WSAGetLastError()));
}

#endif

} 

TcpConnection::TcpConnection(TcpAcceptedConnection accepted) : accepted_(std::move(accepted)) {}

TcpConnection::~TcpConnection() {
    close();
}

TcpConnection::TcpConnection(TcpConnection&& other) noexcept : accepted_(std::move(other.accepted_)) {
    other.accepted_.native_handle = 0;
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept {
    if (this != &other) {
        close();
        accepted_ = std::move(other.accepted_);
        other.accepted_.native_handle = 0;
    }
    return *this;
}

core::Result<core::ByteBuffer> TcpConnection::read_some(std::size_t max_bytes) {
    if (!open()) {
        return core::make_error(core::ErrorCode::InvalidState, "tcp connection is closed");
    }
    if (max_bytes == 0) {
        return core::make_error(core::ErrorCode::InvalidArgument, "tcp read max_bytes must be positive");
    }

#ifdef _WIN32
    core::ByteBuffer buffer(max_bytes);
    const auto received = recv(static_cast<SOCKET>(accepted_.native_handle), reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
    if (received == 0) {
        close();
        return core::make_error(core::ErrorCode::InvalidState, "tcp connection closed by peer");
    }
    if (received == SOCKET_ERROR) {
        return last_winsock_error("recv");
    }
    buffer.resize(static_cast<std::size_t>(received));
    return buffer;
#else
    (void)max_bytes;
    return core::make_error(core::ErrorCode::InvalidState, "tcp connection read is only implemented for Windows in this MVP");
#endif
}

core::Result<void> TcpConnection::write_all(const core::ByteBuffer& bytes) {
    if (!open()) {
        return core::make_error(core::ErrorCode::InvalidState, "tcp connection is closed");
    }

#ifdef _WIN32
    std::size_t sent_total = 0;
    while (sent_total < bytes.size()) {
        const auto remaining = bytes.size() - sent_total;
        const auto sent = send(static_cast<SOCKET>(accepted_.native_handle), reinterpret_cast<const char*>(bytes.data() + sent_total), static_cast<int>(remaining), 0);
        if (sent == SOCKET_ERROR) {
            return last_winsock_error("send");
        }
        sent_total += static_cast<std::size_t>(sent);
    }
    return core::success();
#else
    (void)bytes;
    return core::make_error(core::ErrorCode::InvalidState, "tcp connection write is only implemented for Windows in this MVP");
#endif
}

core::Result<bool> TcpConnection::readable_now() const {
    if (!open()) {
        return core::make_error(core::ErrorCode::InvalidState, "tcp connection is closed");
    }

#ifdef _WIN32
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(static_cast<SOCKET>(accepted_.native_handle), &read_set);
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    const auto selected = select(0, &read_set, nullptr, nullptr, &timeout);
    if (selected == SOCKET_ERROR) {
        return last_winsock_error("select");
    }
    return selected > 0 && FD_ISSET(static_cast<SOCKET>(accepted_.native_handle), &read_set);
#else
    return core::make_error(core::ErrorCode::InvalidState, "tcp connection readable probe is only implemented for Windows in this MVP");
#endif
}

core::Result<void> TcpConnection::close() {
#ifdef _WIN32
    if (accepted_.native_handle != 0 && static_cast<SOCKET>(accepted_.native_handle) != INVALID_SOCKET) {
        closesocket(static_cast<SOCKET>(accepted_.native_handle));
        accepted_.native_handle = 0;
    }
#else
    accepted_.native_handle = 0;
#endif
    return core::success();
}

bool TcpConnection::open() const noexcept {
    return accepted_.native_handle != 0;
}

const TcpAcceptedConnection& TcpConnection::info() const noexcept {
    return accepted_;
}

TcpListener::~TcpListener() {
    stop();
}

core::Result<void> TcpListener::start(const TcpListenerOptions& options) {
    auto valid = validate_options(options);
    if (!valid.ok()) {
        return valid;
    }
    if (status_.running) {
        return core::make_error(core::ErrorCode::InvalidState, "tcp listener is already running");
    }

#ifdef _WIN32
    WSADATA data{};
    const auto startup_result = WSAStartup(MAKEWORD(2, 2), &data);
    if (startup_result != 0) {
        return core::make_error(core::ErrorCode::InternalError, "WSAStartup failed with error " + std::to_string(startup_result));
    }

    auto native = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (native == INVALID_SOCKET) {
        WSACleanup();
        return last_winsock_error("socket");
    }

    BOOL reuse = TRUE;
    setsockopt(native, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(options.port);
    const auto pton_result = inet_pton(AF_INET, options.bind_address.c_str(), &address.sin_addr);
    if (pton_result != 1) {
        closesocket(native);
        WSACleanup();
        return core::make_error(core::ErrorCode::InvalidArgument, "tcp listener bind_address must be an IPv4 literal");
    }

    if (::bind(native, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        auto error = last_winsock_error("bind");
        closesocket(native);
        WSACleanup();
        return error;
    }

    if (::listen(native, options.backlog) == SOCKET_ERROR) {
        auto error = last_winsock_error("listen");
        closesocket(native);
        WSACleanup();
        return error;
    }

    sockaddr_in actual{};
    int actual_len = sizeof(actual);
    if (getsockname(native, reinterpret_cast<sockaddr*>(&actual), &actual_len) == SOCKET_ERROR) {
        auto error = last_winsock_error("getsockname");
        closesocket(native);
        WSACleanup();
        return error;
    }

    socket_ = static_cast<NativeSocket>(native);
    status_.running = true;
    status_.bind_address = options.bind_address;
    status_.port = ntohs(actual.sin_port);
    return core::success();
#else
    (void)options;
    return core::make_error(core::ErrorCode::InvalidState, "tcp listener is only implemented for Windows in this MVP");
#endif
}

core::Result<TcpConnection> TcpListener::accept_once() {
    if (!status_.running) {
        return core::make_error(core::ErrorCode::InvalidState, "tcp listener is not running");
    }

#ifdef _WIN32
    sockaddr_in remote{};
    int remote_len = sizeof(remote);
    const auto accepted = accept(static_cast<SOCKET>(socket_), reinterpret_cast<sockaddr*>(&remote), &remote_len);
    if (accepted == INVALID_SOCKET) {
        return last_winsock_error("accept");
    }

    char remote_address[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &remote.sin_addr, remote_address, sizeof(remote_address));

    TcpAcceptedConnection info;
    info.native_handle = static_cast<std::uintptr_t>(accepted);
    info.remote_address = remote_address;
    info.remote_port = ntohs(remote.sin_port);
    return TcpConnection{std::move(info)};
#else
    return core::make_error(core::ErrorCode::InvalidState, "tcp listener accept is only implemented for Windows in this MVP");
#endif
}

core::Result<void> TcpListener::stop() {
    if (!status_.running) {
        return core::success();
    }

#ifdef _WIN32
    if (socket_ != 0 && static_cast<SOCKET>(socket_) != INVALID_SOCKET) {
        closesocket(static_cast<SOCKET>(socket_));
        socket_ = 0;
    }
    WSACleanup();
#endif

    status_.running = false;
    status_.port = 0;
    return core::success();
}

TcpListenerStatus TcpListener::status() const noexcept {
    return status_;
}

} 
