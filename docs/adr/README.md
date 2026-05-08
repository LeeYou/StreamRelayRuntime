# 架构决策记录

本目录用于保存 StreamRelayRuntime 的架构决策记录，即 ADR。

## ADR 格式

每份 ADR 应使用以下结构：

```text
# ADR-NNNN: 标题

## 状态

Proposed | Accepted | Superseded | Deprecated

## 背景

我们要解决什么问题？有哪些约束？

## 决策

做出了什么决策？

## 影响

什么变得更容易？什么变得更困难？还剩下哪些风险？
```

## 当前 ADR

- [ADR-0001：使用 C++17 作为语言基线](0001-use-cpp17.md)
- [ADR-0002：使用基于 Envelope 的路由替代硬编码消息 ID](0002-envelope-routing.md)
- [ADR-0003：仅将 ECS 思想作为组合模式保留](0003-ecs-composition-only.md)
- [ADR-0004：分离 Relay 控制面与数据面](0004-separate-relay-control-plane-and-data-plane.md)
- [ADR-0005：使用带 Generation 的 ConnectionRef](0005-use-connection-ref-with-generation.md)
