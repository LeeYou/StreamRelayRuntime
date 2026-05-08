#include <cassert>
#include <chrono>

#include "gateway/connection_manager.h"
#include "protocol/binary_codec.h"
#include "protocol/websocket_frame.h"

int main() {
    streamrelay::protocol::Envelope envelope;
    envelope.service = "control";
    envelope.method = "submit_command";
    envelope.deadline_unix_ms = streamrelay::protocol::to_unix_ms(std::chrono::system_clock::now() + std::chrono::seconds{1});
    envelope.payload = streamrelay::core::ByteBuffer{1, 2, 3};

    auto encoded = streamrelay::protocol::encode_envelope_frame(envelope);
    assert(encoded.ok());
    auto decoded = streamrelay::protocol::decode_envelope_frame(encoded.value(), streamrelay::protocol::FrameLimits{}, streamrelay::protocol::to_unix_ms(std::chrono::system_clock::now()));
    assert(decoded.ok());
    assert(decoded.value().payload == envelope.payload);

    streamrelay::protocol::WebSocketFrame ws;
    ws.masked = true;
    ws.masking_key = 0x01020304;
    ws.payload = streamrelay::core::ByteBuffer{4, 5, 6};
    auto ws_encoded = streamrelay::protocol::encode_websocket_frame(ws);
    assert(ws_encoded.ok());
    auto ws_decoded = streamrelay::protocol::decode_websocket_frame(ws_encoded.value(), streamrelay::protocol::WebSocketFrameLimits{});
    assert(ws_decoded.ok());
    assert(ws_decoded.value().payload == ws.payload);

    streamrelay::gateway::ConnectionManagerOptions options;
    options.gateway_id = "gateway-a";
    options.shard_count = 2;
    streamrelay::gateway::ConnectionManager manager{options};
    const auto now = std::chrono::steady_clock::now();
    auto first = manager.accept_connection("websocket", "admin", now);
    auto second = manager.accept_connection("websocket", "device", now);
    assert(first.ok());
    assert(second.ok());
    assert(first.value().connection_id != second.value().connection_id);
    return 0;
}
