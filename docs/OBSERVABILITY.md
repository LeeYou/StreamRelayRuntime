# 可观测性设计

## 1. 目标

StreamRelayRuntime 必须默认可观测。运维人员应能回答：

- 哪个服务实例处理了某个请求？
- 命令为什么失败或超时？
- 哪个 Gateway 持有某个设备连接？
- 哪个 Relay Channel 正在消耗带宽？
- 队列是否正在积压？
- 慢客户端是否造成内存压力？

## 2. Trace Context

每个 Envelope 必须包含：

- `trace_id`
- `request_id`
- `source`
- `target`
- `service`
- `method`
- 可选 `session_id`
- 可选 `device_id`
- 可选 `command_id`
- 可选 `channel_id`

## 3. 日志

日志必须结构化。

推荐字段：

```text
timestamp
level
service
instance_id
trace_id
request_id
session_id
connection_id
device_id
command_id
channel_id
event
error_code
message
```

## 4. Metrics

## 4.1 Gateway Metrics

- `gateway_connections_total`
- `gateway_connections_active`
- `gateway_frames_in_total`
- `gateway_frames_out_total`
- `gateway_invalid_frames_total`
- `gateway_write_queue_bytes`
- `gateway_backpressure_events_total`

## 4.2 Session Metrics

- `session_create_total`
- `session_close_total`
- `session_bind_total`
- `session_bind_fail_total`
- `session_active`
- `session_redis_latency_ms`

## 4.3 Device Registry Metrics

- `device_online_total`
- `device_heartbeat_total`
- `device_lookup_total`
- `device_lookup_fail_total`
- `device_offline_total`

## 4.4 Control Metrics

- `command_submit_total`
- `command_success_total`
- `command_failed_total`
- `command_timeout_total`
- `command_latency_ms`
- `command_state_transition_total`

## 4.5 Relay Metrics

- `relay_channels_active`
- `relay_channels_open_total`
- `relay_channels_closed_total`
- `relay_bytes_up_total`
- `relay_bytes_down_total`
- `relay_backpressure_total`
- `relay_duration_ms`

## 5. Tracing Spans

推荐 span：

```text
gateway.receive_frame
protocol.decode_envelope
session.bind_connection
router.route_envelope
device_registry.lookup
control.submit_command
control.dispatch_command
relay.open_channel
relay.forward_frame
transport.write_frame
```

## 6. 健康检查

每个服务必须暴露：

- liveness check。
- readiness check。
- dependency health check。
- build/version metadata。

## 7. 诊断事件

重要诊断事件：

- connection accepted。
- connection closed。
- authentication failed。
- session bound。
- device registered。
- device offline。
- command state transition。
- relay opened。
- relay closed。
- backpressure triggered。
- message deadline exceeded。

## 8. 告警候选项

- Gateway 活跃连接数超过容量。
- 非法 frame 比例突增。
- Redis 延迟超过阈值。
- 命令超时率超过阈值。
- Relay 背压率超过阈值。
- Router 队列深度超过阈值。
- 设备心跳丢失突增。

## 9. 深化指标补充

### 9.1 Gateway 补充指标

- `gateway_connection_accept_latency_ms`
- `gateway_connection_shard_load`
- `gateway_event_loop_lag_ms`
- `gateway_write_queue_dropped_bytes`
- `gateway_ws_handshake_fail_total`
- `gateway_tls_handshake_fail_total`

### 9.2 Relay 数据面补充指标

- `relay_frame_latency_ms`
- `relay_frame_drop_total`
- `relay_channel_backpressure_duration_ms`
- `relay_channel_close_reason_total`
- `relay_cross_gateway_bytes_total`
- `relay_zero_copy_ratio`

### 9.3 Control 补充指标

- `command_dispatch_latency_ms`
- `command_ack_latency_ms`
- `command_run_duration_ms`
- `command_approval_pending_total`
- `command_device_offline_total`
- `command_policy_denied_total`

### 9.4 Registry 补充指标

- `device_presence_ttl_refresh_total`
- `device_stale_disconnect_ignored_total`
- `device_generation_conflict_total`
