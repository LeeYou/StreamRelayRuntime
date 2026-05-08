# 工程协作规范

## 1. 工程原则

- **设计先于实现**：编码前必须先明确架构、协议和模块契约。
- **接口优先开发**：业务模块依赖接口，而不是具体基础设施实现。
- **小粒度 PR**：每个 PR 聚焦一个目标，并提供清晰测试证据。
- **禁止隐藏式全局依赖**：领域逻辑中避免全局单例访问。
- **资源所有权必须明确**：使用 RAII 与智能指针。
- **所有异步流程必须有边界**：需要 deadline、timeout、cancel 或幂等机制。

## 2. C++ 标准

- 以 **C++17** 为基线。
- 优先使用标准库能力。
- 独占所有权使用 `std::unique_ptr`。
- 只有共享不可变 buffer 或明确共享生命周期时才使用 `std::shared_ptr`。
- 使用 `std::weak_ptr` 打破循环引用。
- 禁止原始 owning pointer。
- 除非文档明确说明，否则异常不得跨模块边界传播。

## 3. 命名规范

| 对象 | 规范 | 示例 |
|---|---|---|
| Namespace | lower_snake_case | `streamrelay::gateway` |
| Class | PascalCase | `ConnectionManager` |
| Interface | `I` 前缀 | `ISessionStore` |
| Function | lower_snake_case | `bind_connection` |
| Variable | lower_snake_case | `connection_id` |
| Member variable | 下划线后缀 | `session_store_` |
| Enum class | PascalCase | `ConnectionState` |
| Enum value | PascalCase | `ConnectionState::Active` |

## 4. 仓库目录规划

```text
StreamRelayRuntime/
  CMakeLists.txt
  cmake/
  docs/
  proto/
  src/
    core/
    runtime/
    transport/
    protocol/
    messaging/
    gateway/
    session/
    device_registry/
    control/
    relay/
    storage/
    observability/
  apps/
    allinone/
    gateway/
    router/
    sessiond/
    controld/
    relayd/
  tests/
    unit/
    integration/
  tools/
```

## 5. 分支策略

- `main`：始终稳定、可发布。
- `develop`：如需要可作为集成分支。
- `feature/<topic>`：功能分支。
- `fix/<topic>`：缺陷修复分支。
- `docs/<topic>`：纯文档分支。

## 6. Commit Message 格式

使用 Conventional Commits：

```text
<type>(<scope>): <summary>

<body>
```

允许的 type：

- `feat`
- `fix`
- `docs`
- `refactor`
- `test`
- `build`
- `ci`
- `chore`

示例：

```text
feat(gateway): add websocket connection lifecycle design
```

## 7. Pull Request 要求

每个 PR 必须包含：

- **目的**：解决什么问题。
- **设计影响**：影响哪些模块和接口。
- **测试证据**：单元测试、集成测试或明确说明。
- **风险评估**：兼容性、性能、安全性和迁移影响。
- **文档更新**：行为或架构变化时必须更新文档。

## 8. Code Review 检查项

- **正确性**：逻辑满足文档契约。
- **所有权**：不存在原始 owning pointer 或不清晰生命周期。
- **并发**：跨线程访问使用 mailbox/message bus 或明确同步机制。
- **错误处理**：失败返回结构化 `Result` 或文档化错误响应。
- **超时**：异步操作具备 deadline。
- **背压**：IO 路径具备有界队列。
- **可观测性**：关键路径输出日志、metrics 和 trace 元数据。
- **测试**：核心逻辑具备确定性测试。

## 9. 测试策略

### 单元测试

以下能力必须有单元测试：

- 路由策略。
- 命令状态机。
- Relay 背压策略。
- Session generation 处理。
- 协议校验。
- 数据结构不变量。

### 集成测试

以下链路必须有集成测试：

- Gateway 会话绑定。
- 命令分发到模拟设备。
- Relay 通道打开/关闭。
- 重连行为。
- Redis 会话过期。

### 性能测试

生产就绪前必须测试：

- 单 Gateway 并发连接数。
- Relay 吞吐。
- 命令延迟 p95/p99。
- 慢客户端下内存增长。
- Router 吞吐。

## 10. 文档策略

- 架构变化需要更新 ADR。
- 新公共接口需要更新模块设计。
- 协议变化需要更新协议设计。
- 运维行为变化需要更新可观测性或安全文档。

## 11. Definition of Done

任务完成必须满足：

- 代码或文档变更完成。
- 提供测试或评审证据。
- 接口已文档化。
- 错误处理和可观测性已考虑。
- PR 通过 CI。
