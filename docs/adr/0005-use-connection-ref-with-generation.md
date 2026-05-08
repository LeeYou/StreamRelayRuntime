# ADR-0005：使用带 Generation 的 ConnectionRef

## 状态

Accepted

## 背景

在分布式长连接系统中，单纯的 `connection_id` 无法唯一表达连接位置。设备或用户快速重连时，旧连接断开事件可能晚于新连接绑定事件到达。如果没有 generation，系统可能错误删除新连接的在线状态。

## 决策

所有跨模块、跨服务的连接引用必须使用 `ConnectionRef`：

```cpp
struct ConnectionRef {
    std::string gateway_id;
    std::uint64_t connection_id;
    std::uint32_t generation;
};
```

任何会影响会话、设备在线状态、命令目标或 Relay endpoint 的操作，都必须校验 generation。

## 影响

- 可防止过期断线事件污染新连接状态。
- Session、Device Registry、Relay、Control 的接口需要统一使用 ConnectionRef。
- 日志和审计可准确定位连接所在 Gateway。
- 所有 Gateway 必须保证本地 connection generation 单调递增或在重用 ID 时递增。
