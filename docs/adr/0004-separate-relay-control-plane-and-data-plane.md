# ADR-0004：分离 Relay 控制面与数据面

## 状态

Accepted

## 背景

StreamRelayRuntime 的核心场景之一是 WebSocket 转发服务。Relay 通道会承载大量高频二进制帧。如果每个数据帧都经过完整 Envelope、Router 和 Relay Service 业务层，会带来过多序列化、内存分配、队列跳转和延迟。

同时，Relay 通道又必须具备授权、审计、统计、限流和关闭语义。这些能力更适合放在控制面，而不是每帧数据热路径。

## 决策

Relay 设计必须分离为：

- **Relay Control Plane**：负责 open/close、授权、策略、通道元数据、审计和统计聚合。
- **Relay Data Plane**：负责轻量帧转发、背压、流控、本地快速转发和跨 Gateway tunnel。

控制面继续使用 Envelope；数据面使用轻量 Relay Frame Header。

## 影响

- WebSocket 转发热路径更短，性能更可控。
- Relay 授权和审计仍然集中管理。
- Gateway 需要承担本地数据面转发能力。
- 设计复杂度高于单一 Relay Service 中转模式。
- 后续可以平滑扩展独立 Relay Node。
