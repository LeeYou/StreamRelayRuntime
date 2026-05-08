# 路线图

## Phase 0：设计基线

### 目标

- 建立产品范围。
- 定义架构和模块边界。
- 定义协议契约。
- 定义工程协作流程。

### 交付物

- `README.md`
- `docs/PRODUCT_VISION.md`
- `docs/ARCHITECTURE.md`
- `docs/MODULE_DESIGN.md`
- `docs/PROTOCOL_DESIGN.md`
- `docs/DATA_MODEL_AND_ALGORITHMS.md`
- `docs/ENGINEERING_GUIDELINES.md`
- `docs/SECURITY_DESIGN.md`
- `docs/OBSERVABILITY.md`
- ADR 模板与初始决策。

## Phase 1：Runtime Skeleton

### 目标

- 创建 CMake 项目结构。
- 实现 core 基础类型。
- 实现 module 生命周期。
- 实现配置与日志基线。
- 实现用于测试的内存版 message bus。

### 验收标准

- 项目可在 Windows 和 Linux 构建。
- 存在单元测试 target。
- 示例 all-in-one app 可以干净启动和停止。
- module 生命周期测试通过。

## Phase 2：Protocol、Gateway 与连接分片 MVP

### 目标

- 定义 protobuf 文件。
- 实现 Envelope codec。
- 实现 WebSocket Gateway。
- 实现 Connection Manager。
- 实现 ConnectionShard 分片模型。
- 实现 ConnectionRef 生成与校验。
- 实现基础心跳。

### 验收标准

- 管理端可以通过 WebSocket 连接。
- 设备 Agent 模拟器可以通过 WebSocket 连接。
- Gateway 校验 frame size 与 envelope deadline。
- 非法 frame 被安全拒绝。
- 连接按 shard 分配，写队列具备 soft/hard limit。

## Phase 3：Session 与 Device Registry

### 目标

- 实现 Session Service。
- 实现 Redis-backed Session Store。
- 实现 Device Registry。
- 实现重连 generation 处理。

### 验收标准

- 设备可以认证并注册。
- 设备在线状态具备 TTL。
- 过期断线事件不会移除新连接。
- Device lookup 返回 gateway 和 connection 位置。

## Phase 4：Relay Control 与本地数据面 MVP

### 目标

- 实现 Relay Control Plane。
- 实现 Relay Channel 创建/关闭。
- 实现轻量 Relay Frame Header。
- 实现同 Gateway 本地 Relay 数据面。
- 实现 soft/hard limit 背压策略。

### 验收标准

- 管理端与设备可以交换二进制帧。
- 数据面不经过 Router 高频热路径。
- 慢接收端触发背压。
- 达到 hard limit 时安全关闭通道。
- Relay 统计可观测。

## Phase 5：远程命令 MVP

### 目标

- 实现增强命令状态机。
- 实现策略检查与可选审批状态。
- 实现命令分发到设备。
- 实现 ACK/result/streaming output 处理。
- 实现超时、取消、拒绝处理。
- 持久化命令审计记录。

### 验收标准

- 管理端可以向在线设备提交命令。
- 设备模拟器收到命令并返回结果。
- 超时命令进入终态。
- 未授权命令被拒绝并写入审计。
- 审计日志被持久化。

## Phase 6：分布式部署

### 目标

- 将服务拆分为独立进程。
- 实现服务注册。
- 实现路由策略。
- 实现跨 Gateway Relay tunnel。
- 增加集成测试环境。

### 验收标准

- 多个 Gateway 可以同时运行。
- Device Registry 能解析正确的 Gateway 位置。
- Router 能选择健康服务实例。
- 跨 Gateway Relay 能双向转发。
- 服务重启不会破坏会话状态。

## Phase 7：生产加固

### 目标

- TLS 支持。
- 授权策略。
- 远程控制安全与审计加固。
- OpenTelemetry 集成。
- Prometheus metrics endpoint。
- 负载测试和故障注入测试。

### 验收标准

- 安全测试套件通过。
- 输出负载测试报告。
- 定义并测量 p95 命令延迟和 Relay 吞吐目标。
- 存在运维 Runbook。
