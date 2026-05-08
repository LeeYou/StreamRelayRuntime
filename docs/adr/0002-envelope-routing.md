# ADR-0002：使用基于 Envelope 的路由替代硬编码消息 ID

## 状态

Accepted

## 背景

参考系统使用数字消息 ID 和 Tag 路由。这种方式效率较高，但会把路由、协议身份和业务处理强耦合。StreamRelayRuntime 需要服务/方法路由、可观测性元数据、授权上下文、deadline、租户/会话/设备身份等通用后台能力。

## 决策

使用版本化 `Envelope` 作为统一路由单元。Envelope 包含 service、method、target、source、request ID、trace ID、session ID、deadline、labels、payload type 和二进制 payload。

## 影响

- 路由表达能力更强，更容易演进。
- 可观测性和安全元数据成为一等公民。
- 相比纯数字消息 ID，协议开销略高。
- 热路径后续可将 service/method 字符串优化为 interned ID 或生成式数字路由 ID。
