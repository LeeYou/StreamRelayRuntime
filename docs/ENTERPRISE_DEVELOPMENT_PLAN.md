# 企业级开发计划与进度跟踪

## 1. 文档目的

本文档用于将 StreamRelayRuntime 从当前内存 MVP 推进到可部署、可观测、可审计、可扩展的企业级运行时平台。

本文档重点回答：

- 当前已经完成了什么。
- 后续需要交付什么。
- 每个阶段的工程验收标准是什么。
- 如何跟踪进度、质量、风险和发布 readiness。
- 哪些事项必须进入企业级生产加固范围。

## 2. 当前项目基线

### 2.1 当前实现状态

截至当前开发进度，项目已经完成 Phase 1 到 Phase 12 的无外部依赖内存 MVP。其中 Phase 7 到 Phase 12 当前是企业级能力的内存内核和可测试闭环，不代表真实生产部署已经完成。

| 阶段 | 状态 | 当前实现摘要 | 验证状态 |
|---|---|---|---|
| Phase 0：设计基线 | 已完成 | 产品、架构、模块、协议、数据模型、安全、观测性和 ADR 文档基线 | 文档已建立 |
| Phase 1：Runtime Skeleton | 已完成 | CMake、core 类型、module 生命周期、AppContext、内存 MessageBus、all-in-one app | CTest 通过 |
| Phase 2：Protocol/Gateway MVP | 已完成 | Envelope codec、WebSocket frame codec、GatewayRuntime、ConnectionManager、ConnectionShard、WriteQueue、heartbeat | CTest 通过 |
| Phase 3：Session/Device Registry MVP | 已完成 | InMemorySessionService、InMemoryDeviceRegistry、Session/Device Registry store port、TTL、presence、generation stale safety | CTest 通过 |
| Phase 4：Relay MVP | 已完成 | RelayFrame、RelayChannelManager、LocalRelayDataPlane、soft/hard backpressure、stats | CTest 通过 |
| Phase 5：Remote Command MVP | 已完成 | Command 状态机、Command/Audit port、内存 store/audit、策略、审批、dispatch、ack、result、timeout、cancel | CTest 通过 |
| Phase 6：Distributed Core MVP | 已完成 | InMemoryServiceRegistry、Router、GatewayTunnelManager、跨 Gateway frame 内存转发 | CTest 通过 |
| Phase 7：Production Hardening Core MVP | MVP 已完成 | InMemoryTransportServer、Session/Device Registry store port、Command/Audit port、MetricsRegistry、InMemoryLogSink、模块化 CTest target、Debug/Release 验证 | CTest 通过 |
| Phase 8：Gateway Access Core MVP | MVP 已完成 | GatewayEdge 接入编排、TcpListener Windows MVP、同步 accept/read/write MVP、WebSocket handshake codec、WebSocketGatewayAdapter、WebSocketGatewayListener、token 认证、admin/device accept、session/device register、WebSocket frame 解包后交给 GatewayRuntime | CTest 通过 |
| Phase 9：Cross-Gateway Relay Core MVP | MVP 已完成 | GatewayTunnelBridge 将 GatewayTunnelManager outbound frame 泵送到远端 inbound queue，并支持向 LocalRelayDataPlane 交付 | CTest 通过 |
| Phase 10：Security/Auth Core MVP | MVP 已完成 | StaticTokenAuthenticator、AuthorizationPolicy、SecureControlService，将 token/RBAC 与 ControlService submit_command 串联 | CTest 通过 |
| Phase 11：Observability/SLO Core MVP | MVP 已完成 | PrometheusExporter、HealthRegistry、InMemoryTracer、SloRegistry，支持 metrics 文本导出、health/readiness、trace span、SLO 判定 | CTest 通过 |
| Phase 12：Ops/Release Core MVP | MVP 已完成 | RuntimeConfigValidator、ReleaseManifestRenderer、GitHub Actions CI workflow，支持配置校验、发布清单渲染、Windows Debug/Release CI 基线 | CTest 通过 |

### 2.2 当前已验证命令

```powershell
cmake -S . -B build -DSTREAMRELAY_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

当前验证结果：

```text
100% tests passed, 0 tests failed out of 13
```

### 2.3 当前重要边界

当前系统仍是无外部依赖的内存内核，不代表生产可用版本。

尚未完成：

- 真实 TCP socket accept/read/write event loop。
- Redis-backed Session Store 和 Device Registry Adapter 实现。
- Command DB 和 Audit DB 持久化 adapter 实现。
- 服务发现后端。
- 跨 Gateway 真实网络 tunnel。
- TLS/mTLS。
- 完整认证授权 IAM/RBAC/ABAC。
- OpenTelemetry exporter。
- 真实 HTTP Prometheus `/metrics` endpoint。
- 集成测试环境。
- 压测、故障注入和安全测试。
- 完整 CD 发布流水线。

### 2.4 最新交付物快照

Sprint 1 和 Phase 7 到 Phase 12 MVP 已落地以下文件：

| 范围 | 交付物 |
|---|---|
| 测试结构 | `tests/unit/runtime_core_tests.cpp`、`protocol_gateway_tests.cpp`、`session_device_tests.cpp`、`store_port_tests.cpp`、`command_store_port_tests.cpp`、`relay_router_tests.cpp`、`control_tests.cpp`、`enterprise_mvp_tests.cpp` |
| P7 Transport/Observability | `src/transport/in_memory_transport.*`、`src/observability/metrics.*` |
| P7 Store Ports | `src/session/session_store.*`、`src/device_registry/device_registry_store.*` |
| P7 Command/Audit Ports | `src/control/command_store.*` |
| P8 Gateway 接入编排 | `src/gateway/gateway_edge.*`、`src/gateway/websocket_gateway_adapter.*`、`src/gateway/websocket_gateway_listener.*`、`src/transport/websocket_handshake.*`、`src/transport/tcp_listener.*` |
| P9 Relay Tunnel Bridge | `src/relay/gateway_tunnel_bridge.*` |
| P10 Security/Auth | `src/security/auth.*`、`src/control/secure_control_service.*` |
| P11 Observability/SLO | `src/observability/prometheus_exporter.*`、`health.*`、`trace.*`、`slo.*` |
| P12 Ops/Release | `src/ops/runtime_config.*`、`release_manifest.*`、`.github/workflows/ci.yml` |

当前验证命令：

```powershell
cmake -S . -B build -DSTREAMRELAY_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## 3. 企业级目标架构交付原则

### 3.1 控制面与数据面分离

- 控制面使用 Envelope、Router、Session、DeviceRegistry、Control、RelayControl。
- Relay 高频数据面不经过 Router 热路径。
- Relay 数据面使用轻量 Relay Frame Header。
- Gateway 内本地转发优先走 LocalRelayDataPlane。
- 跨 Gateway 转发走 GatewayTunnel。

### 3.2 所有连接引用必须带 generation

所有跨模块连接引用统一使用：

```cpp
ConnectionRef(gateway_id, connection_id, generation)
```

必须保证：

- 旧断线事件不能关闭新连接。
- 旧 session/device presence 不能覆盖新 generation。
- Router、Relay、Control 只能使用带 generation 的连接引用。

### 3.3 企业级质量门禁

每个阶段必须满足：

- 单元测试通过。
- 新核心状态机有正向、反向、非法状态测试。
- 所有外部输入有 size/deadline/state 校验。
- 无无界队列。
- 有明确错误码。
- 有审计或指标事件。
- 可被集成测试验证。

## 4. 后续阶段计划

## 4.1 Phase 7：生产加固基础

当前状态：MVP 已完成。已实现内存 transport、Session/Device Registry store port 与内存 adapter、Command/Audit 持久化 port 与内存 adapter、基础 metrics/log sink、模块化测试 target、Debug/Release 构建验证。真实外部存储 adapter、CI/CD 发布增强仍属于后续生产化工作。

### 目标

将当前内存 MVP 加固为可接真实网络和真实存储的工程基础。

### 工作项

| 编号 | 工作项 | 优先级 | 输出物 | 验收标准 |
|---|---|---:|---|---|
| P7-1 | 引入 transport 抽象 | P0 | `src/transport/` 接口 | GatewayRuntime 不直接依赖具体 socket 实现 |
| P7-2 | 实现真实 WebSocket Gateway Adapter | P0 | WebSocket server adapter | 管理端和设备 Agent 可真实连接 |
| P7-3 | Redis adapter 接口设计 | P0 | Session/Device Registry store port | 内存实现与 Redis 实现可替换 |
| P7-4 | Command/Audit 持久化接口 | P0 | CommandStore/AuditStore port | 命令重启后可恢复 |
| P7-5 | 结构化日志基础 | P1 | logging module | 日志包含 service、trace、request、session、device、command、connection |
| P7-6 | Metrics registry | P1 | metrics module | 连接数、队列长度、命令状态、Relay bytes/frames 有 counter/gauge |
| P7-7 | 错误码规范化 | P1 | error taxonomy | Gateway/Router/Control/Relay 错误码可稳定对外映射 |

### 验收门禁

- Debug 和 Release 构建通过。
- 所有现有 CTest 通过。
- 新增 transport adapter 有最小集成测试。
- Redis/DB adapter 可以通过 mock 或 local 测试替换内存实现。
- 日志和 metrics 可在 all-in-one app 中输出。

## 4.2 Phase 8：真实 Gateway 与设备接入

当前状态：内核 MVP 已完成。已实现 `GatewayEdge`，支持 token 认证、admin/device accept、session 创建、device register/heartbeat、WebSocket binary frame 解包并转交 `GatewayRuntime`。已实现 `WebSocketHandshakeCodec` 和 `WebSocketGatewayAdapter`，支持 HTTP Upgrade 握手解析、`Sec-WebSocket-Accept` 生成、admin/device 握手接入。已实现 Windows `TcpListener` MVP，支持 bind/listen、端口 0 自动分配、状态查询、同步 `accept_once`、`read_some`、`write_all` 和停止释放资源。已实现 `WebSocketGatewayListener` 同步 MVP，可从真实 TCP 连接读取握手请求、调用 Gateway adapter，并写回 101 响应。Debug/Release 全量 CTest 已通过。

### 目标

让管理端和设备 Agent 能通过真实 WebSocket 接入系统，并完成认证、session 创建、device register、heartbeat、命令下发闭环。

### 工作项

| 编号 | 工作项 | 优先级 | 输出物 | 验收标准 |
|---|---|---:|---|---|
| P8-1 | Gateway WebSocket listener | P0 | gateway app | 支持 admin/device 两类连接 |
| P8-2 | Client handshake 协议 | P0 | auth/register envelope handlers | 非法握手被拒绝 |
| P8-3 | 设备注册流程 | P0 | register_device handler | DeviceRegistry 返回正确 gateway/connection/generation |
| P8-4 | 心跳流程 | P0 | heartbeat handler | TTL 自动续期，过期后 offline |
| P8-5 | 命令端到端链路 | P0 | submit_command -> device.execute -> result | 管理端可收到命令结果 |
| P8-6 | Gateway 写回响应 | P1 | response writer | command ack/result 可回到正确连接 |
| P8-7 | 连接异常清理 | P1 | disconnect handlers | stale generation 不影响新连接 |

### 验收门禁

- 两个模拟客户端可以真实接入。
- 在线设备 lookup 返回正确 Gateway 位置。
- 管理端提交命令，设备模拟器收到并返回结果。
- 断开重连后旧断线事件不会移除新 presence。
- 非法 frame、过期 envelope、超限 payload 被安全拒绝。

## 4.3 Phase 9：Relay 跨 Gateway 数据面

当前状态：内核 MVP 已完成。已实现 `GatewayTunnelBridge`，可以把一个 Gateway tunnel 的 outbound frame 泵送到另一个 Gateway tunnel 的 inbound queue，并支持向本地 Relay data plane 交付。真实跨进程 tunnel transport、重连和窗口流控仍待实现。

### 目标

从当前内存 GatewayTunnelManager 推进到真实 Gateway-to-Gateway tunnel，实现跨 Gateway 双向数据转发。

### 工作项

| 编号 | 工作项 | 优先级 | 输出物 | 验收标准 |
|---|---|---:|---|---|
| P9-1 | Gateway Tunnel transport | P0 | tunnel adapter | 两个 Gateway 建立内部连接 |
| P9-2 | RelayControl open/close API | P0 | relay control handlers | 通道创建、关闭可审计 |
| P9-3 | 跨 Gateway RelayFrame 转发 | P0 | tunnel data plane | 管理端与设备跨 Gateway 双向转发 |
| P9-4 | Window/credit 初版流控 | P1 | flow-control module | 慢接收端触发 backpressure |
| P9-5 | Relay stats 聚合 | P1 | stats endpoint/store | channel bytes/frames/duration 可查询 |
| P9-6 | Tunnel reconnect | P1 | reconnect policy | Gateway 短暂断线可恢复或安全关闭通道 |

### 验收门禁

- Gateway A 和 Gateway B 独立进程运行。
- Admin 连接 Gateway A，Device 连接 Gateway B，二进制帧可双向转发。
- hard limit 关闭通道并写入 close reason。
- Relay stats 可观测。

## 4.4 Phase 10：安全、授权与审计加固

当前状态：内核 MVP 已完成。已实现静态 token 认证、基于 role/resource/action 的授权策略，以及 `SecureControlService`，将认证授权与命令提交串联。TLS/mTLS、JWT/OIDC、完整 IAM/RBAC/ABAC、持久化审计和防重放仍待实现。

### 目标

满足远程控制场景的企业安全要求。

### 工作项

| 编号 | 工作项 | 优先级 | 输出物 | 验收标准 |
|---|---|---:|---|---|
| P10-1 | TLS/mTLS | P0 | TLS config | 外部和内部连接均可启用 TLS |
| P10-2 | Token 验证 | P0 | auth module | 过期、篡改、缺失 token 被拒绝 |
| P10-3 | RBAC/ABAC 策略 | P0 | policy engine | command/relay 操作必须授权 |
| P10-4 | 高风险命令审批 | P0 | approval workflow | 高风险命令未经审批不能下发 |
| P10-5 | 审计持久化 | P0 | audit store | 命令、Relay、登录、拒绝事件可查询 |
| P10-6 | 防重放 | P1 | nonce/idempotency/replay guard | 重放 envelope/command 被拒绝 |
| P10-7 | Secret 管理 | P1 | config secret port | 密钥不硬编码，不输出到日志 |

### 验收门禁

- 未授权命令被拒绝并写入审计。
- 审计事件可按 command_id、operator_id、device_id 查询。
- 安全测试套件通过。
- 日志中不包含 token、secret、敏感 payload。

## 4.5 Phase 11：可观测性、压测与 SLO

当前状态：内核 MVP 已完成。已实现 Prometheus 文本渲染、health/readiness 状态聚合、内存 trace span、SLO target/measurement 判定。真实 HTTP endpoint、OpenTelemetry exporter、压测工具和故障注入仍待生产化实现。

### 目标

建立生产运行所需的 metrics、tracing、logging、健康检查和性能基线。

### 工作项

| 编号 | 工作项 | 优先级 | 输出物 | 验收标准 |
|---|---|---:|---|---|
| P11-1 | Prometheus metrics endpoint | P0 | `/metrics` | 核心指标可抓取 |
| P11-2 | OpenTelemetry tracing | P0 | trace propagation | Envelope trace_id 贯穿 Gateway/Router/Control/Relay |
| P11-3 | 健康检查 | P0 | `/healthz` `/readyz` | 依赖不可用时 ready=false |
| P11-4 | 压测工具 | P1 | benchmark tool | 可模拟连接数、命令 QPS、Relay 吞吐 |
| P11-5 | 故障注入 | P1 | fault tests | Redis/Gateway/Tunnel 断开场景可验证 |
| P11-6 | SLO 定义 | P1 | SLO 文档 | p95 command latency、relay throughput、error rate 有目标 |

### 初始 SLO 建议

| 指标 | MVP 目标 | 企业生产目标 |
|---|---:|---:|
| Gateway 单进程连接数 | 1,000 | 50,000+ |
| 命令提交 p95 | < 200ms | < 100ms |
| 命令端到端 p95 | < 1s | < 500ms |
| Relay 本地转发 p95 | < 20ms | < 10ms |
| Router 路由 p95 | < 10ms | < 5ms |
| 控制面错误率 | < 1% | < 0.1% |

## 4.6 Phase 12：发布工程与运维体系

当前状态：内核 MVP 已完成。已实现 runtime 配置校验、release manifest 文本渲染和 GitHub Actions Windows Debug/Release CI 基线。完整 artifact packaging、容器化、部署模板、CD 发布、Runbook 和回滚自动化仍待实现。

### 目标

建立企业级持续交付、部署、回滚、运维和应急体系。

### 工作项

| 编号 | 工作项 | 优先级 | 输出物 | 验收标准 |
|---|---|---:|---|---|
| P12-1 | CI pipeline | P0 | build/test workflow | PR 自动构建和测试 |
| P12-2 | Release pipeline | P0 | artifact/package | 可生成版本化产物 |
| P12-3 | 配置管理 | P0 | config schema | 环境配置可验证 |
| P12-4 | 容器化 | P1 | Dockerfile/compose | all-in-one 与多服务可容器运行 |
| P12-5 | 部署模板 | P1 | Helm/K8s manifests | Gateway/Router/Control 等可独立部署 |
| P12-6 | Runbook | P1 | 运维手册 | 常见故障有处置流程 |
| P12-7 | 版本兼容策略 | P1 | upgrade guide | 协议和 schema 升级可灰度 |

## 5. 工作流与分支策略

### 5.1 推荐分支模型

- `main`：始终保持可构建、可测试、可发布候选。
- `develop`：阶段集成分支。
- `feature/<area>-<short-name>`：功能分支。
- `fix/<area>-<short-name>`：缺陷修复分支。
- `release/<version>`：发布稳定分支。

### 5.2 Pull Request 门禁

每个 PR 必须包含：

- 变更说明。
- 影响范围。
- 测试说明。
- 风险说明。
- 是否涉及协议/数据模型/安全变更。
- 是否需要新增 ADR。

### 5.3 Definition of Done

功能完成必须满足：

- 代码已合入对应 target。
- 单元测试覆盖核心路径和失败路径。
- 构建和 CTest 通过。
- 文档更新。
- 错误码和日志符合规范。
- 无明显无界资源风险。
- 若涉及安全或协议，已完成设计评审。

## 6. 测试策略

### 6.1 单元测试

目标：覆盖所有核心状态机、codec、store、router、relay、control 逻辑。

必须覆盖：

- 成功路径。
- 非法输入。
- 过期 deadline/TTL。
- stale generation。
- backpressure soft/hard limit。
- terminal state immutable。
- idempotency。

### 6.2 集成测试

目标：验证多模块链路。

建议场景：

- Admin/Device 真实 WebSocket 连接。
- Device register -> heartbeat -> lookup。
- submit command -> dispatch -> ack -> result。
- local relay binary frame round trip。
- cross gateway relay binary frame round trip。
- gateway restart 后 session/device/tunnel 行为。

### 6.3 端到端测试

目标：验证实际用户场景。

建议场景：

- 管理端远程执行命令。
- 高风险命令审批后执行。
- 设备断线重连后继续被正确路由。
- Relay 大文件或持续二进制流转发。
- 慢客户端触发 backpressure。

### 6.4 非功能测试

必须逐步建立：

- 压力测试。
- 长稳测试。
- 故障注入。
- 安全扫描。
- 内存泄漏检查。
- 协议兼容性测试。

## 7. 风险清单

| 风险 | 影响 | 当前状态 | 缓解措施 |
|---|---|---|---|
| 真实网络接入尚未实现 | MVP 不能真实部署 | TcpListener + WebSocket handshake/adapter/listener MVP 已完成，真实事件循环未完成 | 基于 `TcpListener` 和 `WebSocketGatewayListener` 接真实 socket event loop |
| Redis/DB adapter 尚未实现 | 重启后状态丢失 | 未完成 | 抽象 store port，先做 adapter 测试 |
| 安全授权不足 | 远程控制风险高 | 静态 token/RBAC MVP 已完成 | Phase 10 后续实现 TLS、JWT/OIDC、ABAC、防重放、审计持久化 |
| 跨 Gateway tunnel 仅内存模型 | 无法跨进程转发 | bridge MVP 已完成，真实 transport 未完成 | Phase 9 后续实现真实 tunnel transport |
| 可观测性不足 | 生产排障困难 | Metrics/log/Prometheus text/health/trace/SLO MVP 已完成 | Phase 11 后续接真实 HTTP endpoint 与 OpenTelemetry exporter |
| 无 CI | 回归风险高 | GitHub Actions Windows Debug/Release CI 基线已完成 | Phase 12 后续补 artifact、coverage、security scan 和 CD |
| 单元测试文件过大 | 后续维护困难 | 已缓解 | 继续逐步把 `runtime_tests.cpp` 中全量回归拆成独立模块测试 |

## 8. 推荐近期冲刺计划

### Sprint 1：测试结构与工程卫生

当前状态：已完成第一版。已新增 5 个模块化单元测试 target 和 1 个企业 MVP 集成测试 target，并保留原全量 `streamrelay_unit_tests` 作为回归测试。

目标：降低当前快速迭代带来的维护成本。

任务：

- 拆分 `tests/unit/runtime_tests.cpp`。
- 每个模块独立 test file。
- 增加 Release 构建验证。
- 增加基本 CI 脚本。
- 增加 CMake option 分组。

验收：

- `ctest` 仍全部通过。
- 每个模块测试可单独定位。
- Debug/Release 均可构建。

### Sprint 2：Transport 抽象和真实 Gateway

当前状态：内核 MVP 已完成。已实现 `InMemoryTransportServer`、Windows `TcpListener`、同步 accept/read/write MVP、`GatewayEdge`、`WebSocketHandshakeCodec`、`WebSocketGatewayAdapter` 和 `WebSocketGatewayListener`，Debug/Release 全量 CTest 已通过。下一步是接真实事件循环和外部客户端兼容测试。

目标：让系统具备真实连接能力。

任务：

- 定义 `ITransportServer`、`IConnection`、`IFrameHandler`。
- 实现 WebSocket adapter。
- GatewayRuntime 接入 transport adapter。
- 增加 admin/device simulator。

验收：

- 两个模拟客户端可通过 WebSocket 连接。
- Gateway 可读取 frame 并写回响应。
- 非法 frame 被断开或拒绝。

### Sprint 3：存储 Adapter

目标：将内存状态迁移到可替换持久层。

任务：

- 抽象 SessionStore。
- 抽象 DeviceRegistryStore。
- 抽象 CommandStore。已完成 `ICommandStore` 与内存 adapter。
- 抽象 AuditStore。已完成 `ICommandAuditLog` 与内存 adapter。
- 实现 Redis/DB adapter 的 mock 和接口测试。

验收：

- 内存 store 和外部 store 通过同一套契约测试。
- 重启恢复场景可验证。

### Sprint 4：真实命令闭环

目标：完成管理端到设备端的真实远程命令链路。

任务：

- Gateway 认证。
- Device register。
- Command dispatch envelope。
- Device simulator 执行并返回 result。
- Admin response writeback。

验收：

- 管理端可以提交命令。
- 设备模拟器收到命令并返回结果。
- 超时、拒绝、取消均进入正确终态。

## 9. 进度跟踪模板

### 9.1 周度状态模板

```markdown
# 周报：YYYY-MM-DD

## 本周完成

- [ ] 

## 下周计划

- [ ] 

## 当前风险

| 风险 | Owner | 影响 | 缓解措施 | 截止时间 |
|---|---|---|---|---|
| | | | | |

## 构建与测试

- Debug build：通过/失败
- Release build：通过/失败
- CTest：通过/失败
- 集成测试：通过/失败/未执行

## 关键指标

| 指标 | 当前值 | 目标 | 备注 |
|---|---:|---:|---|
| 单元测试数 | | | |
| 覆盖模块数 | | | |
| 已知 P0 bug | | 0 | |
| 命令 p95 | | | |
| Relay 吞吐 | | | |
```

### 9.2 Feature Tracking 模板

```markdown
# Feature：<名称>

## 背景

## 目标

## 非目标

## 设计链接

## 任务拆分

- [ ] 接口设计
- [ ] 核心实现
- [ ] 单元测试
- [ ] 集成测试
- [ ] 文档更新
- [ ] 可观测性
- [ ] 安全评审

## 验收标准

## 风险

## 发布计划
```

### 9.3 Bug Tracking 模板

```markdown
# Bug：<标题>

## 现象

## 影响范围

## 复现步骤

## 根因

## 修复方案

## 回归测试

## 是否需要补充监控或日志
```

## 10. 企业级发布门禁

进入生产候选前必须满足：

- 所有 P0/P1 功能完成。
- 所有 P0/P1 bug 关闭。
- Debug/Release 构建通过。
- 单元测试、集成测试、端到端测试通过。
- 安全测试通过。
- 压测报告完成。
- Runbook 完成。
- 回滚方案完成。
- 监控告警规则完成。
- 审计日志可查询。
- 配置和密钥管理符合安全要求。

## 11. 当前下一步建议

最高优先级建议：

1. 拆分单元测试，降低维护风险。
2. 抽象 transport，接真实 WebSocket Gateway。
3. 抽象 store port，为 Redis/DB adapter 做准备。
4. 建立 CI，防止后续快速迭代回归。
5. 将命令闭环和设备接入做成第一个真实端到端 demo。

推荐立即进入：

```text
Sprint 1：测试结构与工程卫生
Sprint 2：Transport 抽象和真实 Gateway
```
