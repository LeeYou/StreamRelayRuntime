#include <cassert>

#include "transport/tcp_listener.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

int main() {
    streamrelay::transport::TcpListener listener;
    streamrelay::transport::TcpListenerOptions options;
    options.bind_address = "127.0.0.1";
    options.port = 0;
    options.backlog = 16;

    auto started = listener.start(options);
#ifdef _WIN32
    assert(started.ok());
    auto status = listener.status();
    assert(status.running);
    assert(status.bind_address == "127.0.0.1");
    assert(status.port > 0);

    auto client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(client != INVALID_SOCKET);
    DWORD timeout_ms = 2000;
    auto receive_timeout_result = setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    auto send_timeout_result = setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    assert(receive_timeout_result == 0);
    assert(send_timeout_result == 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(status.port);
    auto pton_result = inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    assert(pton_result == 1);
    auto connect_result = connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    assert(connect_result != SOCKET_ERROR);

    auto accepted = listener.accept_once();
    assert(accepted.ok());
    assert(accepted.value().open());
    assert(accepted.value().info().remote_port > 0);

    const char ping[] = "ping";
    auto sent_ping = send(client, ping, 4, 0);
    assert(sent_ping == 4);
    auto received = accepted.value().read_some(16);
    assert(received.ok());
    assert(received.value() == streamrelay::core::ByteBuffer({112, 105, 110, 103}));

    auto write_pong = accepted.value().write_all(streamrelay::core::ByteBuffer({112, 111, 110, 103}));
    assert(write_pong.ok());
    char pong[4]{};
    auto received_pong = recv(client, pong, 4, 0);
    assert(received_pong == 4);
    assert(pong[0] == 'p');
    assert(pong[1] == 'o');
    assert(pong[2] == 'n');
    assert(pong[3] == 'g');
    closesocket(client);
    auto closed_connection = accepted.value().close();
    assert(closed_connection.ok());

    assert(!listener.start(options).ok());
    auto stopped = listener.stop();
    assert(stopped.ok());
    assert(!listener.status().running);
#else
    assert(!started.ok());
#endif

    streamrelay::transport::TcpListener invalid;
    streamrelay::transport::TcpListenerOptions invalid_options;
    invalid_options.bind_address.clear();
    assert(!invalid.start(invalid_options).ok());
    return 0;
}
