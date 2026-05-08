# 协议设计

## 1. 协议目标

- 为所有外部和内部消息提供统一 **Envelope**。
- 通过版本化契约支持向前兼容演进。
- 通过 request ID 和 trace ID 追踪请求。
- 通过显式 deadline 约束异步流程。
- 支持 Relay 与流式场景中的二进制 payload。
- 使用 service/method 路由，替代硬编码游戏式消息 ID。
- 控制面协议与数据面协议分离：Envelope 面向控制面，Relay Frame Header 面向高频数据面。

## 2. 传输模式

| 模式 | 用途 | Payload |
|---|---|---|
| WebSocket text | 调试、管理工具 | JSON envelope |
| WebSocket binary | 生产客户端/Agent 协议 | binary envelope + protobuf |
| TCP binary | Agent 或内部高吞吐路径 | binary envelope + protobuf |
| HTTP/HTTPS | 登录、管理 API、健康检查 | JSON/protobuf |
| Internal RPC | 服务间调用 | protobuf/gRPC 或 runtime envelope |
| Relay data plane | 高频 Relay 帧转发 | lightweight relay frame |

## 3. Envelope Schema

```proto
syntax = "proto3";
package streamrelay.protocol.v1;

message Envelope {
  uint32 version = 1;
  uint64 request_id = 2;
  uint64 trace_id = 3;
  string source = 4;
  string target = 5;
  string service = 6;
  string method = 7;
  string session_id = 8;
  string principal_id = 9;
  int64 deadline_unix_ms = 10;
  map<string, string> labels = 11;
  string payload_type = 12;
  bytes payload = 13;
}

message Error {
  uint32 code = 1;
  string name = 2;
  string message = 3;
  bool retryable = 4;
}
```

## 4. 二进制帧布局

该布局用于 Envelope 控制面消息，不用于高频 Relay 数据帧。

```text
magic:uint16     = 0x5352
version:uint16   = 1
flags:uint32
header_len:uint32
envelope_len:uint32
payload_len:uint32
header_crc:uint32
envelope_bytes
payload_bytes
```

### 校验规则

- `magic` 必须匹配 `0x5352`。
- `version` 必须被当前实现支持。
- `header_len`、`envelope_len`、`payload_len` 必须在配置限制内。
- 开启 CRC flag 时必须校验 CRC。
- 未知 critical flag 必须拒绝该 frame。
- 分发前必须检查 deadline。

## 5. Relay 数据面轻量帧

Relay 数据面不逐帧携带完整 Envelope，而使用轻量帧头：

```text
magic:uint16        = 0x5246
version:uint16      = 1
flags:uint16
header_len:uint16
channel_handle:uint64
sequence:uint64
ack_sequence:uint64
window_credit:uint32
payload_len:uint32
payload_crc:uint32 optional
payload_bytes
```

### flags

- `DATA`
- `ACK`
- `FIN`
- `RESET`
- `WINDOW_UPDATE`
- `COMPRESSED`

### 规则

- `channel_handle` 由 Relay Control 创建通道时分配。
- `sequence` 在单方向内递增。
- `ack_sequence` 与 `window_credit` 用于端到端流控。
- `payload_len` 必须受通道、连接、租户配置限制。
- 控制面仍通过 Envelope 处理 open、close、stats 和 policy。

## 6. 核心服务与方法

## 6.1 Auth

| Service | Method | Request | Response |
|---|---|---|---|
| `auth` | `login_user` | `LoginUserRequest` | `LoginUserResponse` |
| `auth` | `login_device` | `LoginDeviceRequest` | `LoginDeviceResponse` |
| `auth` | `refresh_token` | `RefreshTokenRequest` | `RefreshTokenResponse` |

```proto
message LoginUserRequest {
  string account = 1;
  string credential = 2;
  string tenant_id = 3;
}

message LoginUserResponse {
  Error error = 1;
  string access_token = 2;
  string refresh_token = 3;
  int64 expires_at_unix_ms = 4;
}
```

## 6.2 Session

| Service | Method | Request | Response |
|---|---|---|---|
| `session` | `bind_connection` | `BindConnectionRequest` | `BindConnectionResponse` |
| `session` | `close_session` | `CloseSessionRequest` | `CloseSessionResponse` |
| `session` | `heartbeat` | `SessionHeartbeat` | `SessionHeartbeatAck` |

```proto
message BindConnectionRequest {
  string access_token = 1;
  uint64 connection_id = 2;
  uint32 connection_generation = 3;
  string gateway_id = 4;
  string client_kind = 5;
}

message BindConnectionResponse {
  Error error = 1;
  string session_id = 2;
  string principal_id = 3;
}
```

## 6.3 Device Registry

| Service | Method | Request | Response |
|---|---|---|---|
| `device_registry` | `register` | `RegisterDeviceRequest` | `RegisterDeviceResponse` |
| `device_registry` | `heartbeat` | `DeviceHeartbeat` | `DeviceHeartbeatAck` |
| `device_registry` | `lookup` | `LookupDeviceRequest` | `LookupDeviceResponse` |
| `device_registry` | `mark_offline` | `MarkDeviceOfflineRequest` | `MarkDeviceOfflineResponse` |

```proto
message RegisterDeviceRequest {
  string device_id = 1;
  string session_id = 2;
  uint64 connection_id = 3;
  string gateway_id = 4;
  uint32 connection_generation = 5;
  repeated string capabilities = 6;
  map<string, string> labels = 7;
}

message LookupDeviceResponse {
  Error error = 1;
  string device_id = 2;
  string gateway_id = 3;
  uint64 connection_id = 4;
  uint32 connection_generation = 5;
  repeated string capabilities = 6;
}
```

## 6.4 Control

| Service | Method | Request | Response |
|---|---|---|---|
| `control` | `submit_command` | `SubmitCommandRequest` | `SubmitCommandResponse` |
| `control` | `command_ack` | `CommandAck` | `CommandAckResponse` |
| `control` | `command_result` | `CommandResult` | `CommandResultResponse` |
| `control` | `cancel_command` | `CancelCommandRequest` | `CancelCommandResponse` |

```proto
message SubmitCommandRequest {
  string device_id = 1;
  string operator_id = 2;
  string command_type = 3;
  bytes command_payload = 4;
  int64 timeout_ms = 5;
  string idempotency_key = 6;
  string risk_level = 7;
}

message SubmitCommandResponse {
  Error error = 1;
  string command_id = 2;
  string state = 3;
}

message CommandResult {
  string command_id = 1;
  string device_id = 2;
  int32 exit_code = 3;
  bytes output = 4;
  string state = 5;
}
```

## 6.5 Relay

| Service | Method | Request | Response |
|---|---|---|---|
| `relay` | `open_channel` | `OpenRelayChannelRequest` | `OpenRelayChannelResponse` |
| `relay` | `close_channel` | `CloseRelayChannelRequest` | `CloseRelayChannelResponse` |
| `relay` | `frame` | `RelayFrame` | none |
| `relay` | `stats` | `RelayStatsRequest` | `RelayStatsResponse` |

```proto
message OpenRelayChannelRequest {
  string source_session_id = 1;
  string target_device_id = 2;
  string mode = 3;
  map<string, string> options = 4;
}

message OpenRelayChannelResponse {
  Error error = 1;
  string channel_id = 2;
  fixed64 channel_handle = 3;
}

message RelayFrame {
  string channel_id = 1;
  fixed64 channel_handle = 2;
  uint64 sequence = 3;
  uint64 ack_sequence = 4;
  uint32 window_credit = 5;
  bytes data = 6;
  bool fin = 7;
}
```

生产数据面优先使用轻量帧头；该 protobuf `RelayFrame` 主要用于调试、测试或非热路径。

## 7. 错误码策略

| Code Range | Category |
|---:|---|
| 0 | OK |
| 1000-1999 | 参数与校验错误 |
| 2000-2999 | 认证与授权错误 |
| 3000-3999 | 会话错误 |
| 4000-4999 | 设备注册错误 |
| 5000-5999 | 控制命令错误 |
| 6000-6999 | Relay 错误 |
| 9000-9999 | 内部与基础设施错误 |

## 8. 兼容性规则

- Protobuf 中已移除字段的编号禁止复用。
- 允许追加字段。
- 语义性不兼容变更必须升级 service method 版本。
- 未知 labels 默认忽略，除非策略标记为 critical。
- 旧客户端可使用 JSON envelope 调试，但生产 Agent 应使用二进制帧。
