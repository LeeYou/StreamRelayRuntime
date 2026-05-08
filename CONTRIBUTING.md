# StreamRelayRuntime 贡献指南

## 1. 协作模式

StreamRelayRuntime 采用企业级工程协作流程：

- 设计文档先行。
- Pull Request 保持小粒度、可审查。
- 接口优先实现。
- 核心逻辑必须具备测试。
- 重大技术决策必须记录 ADR。

## 2. 开始开发前

实现功能前请先完成：

1. 阅读 `README.md`。
2. 阅读 `docs/` 下相关设计文档。
3. 检查 `docs/adr/` 下已有架构决策。
4. 如果变更影响架构或接口契约，新增或更新 ADR。

## 3. Pull Request 检查项

每个 PR 应包含：

- **问题说明**：为什么需要这个变更。
- **设计摘要**：变更如何符合模块边界。
- **测试证据**：单元测试、集成测试或人工验证说明。
- **风险分析**：兼容性、安全性、性能和运维影响。
- **文档更新**：架构、模块或协议变化必须同步更新文档。

## 4. Commit 规范

使用 Conventional Commits：

```text
feat(scope): summary
fix(scope): summary
docs(scope): summary
refactor(scope): summary
test(scope): summary
build(scope): summary
ci(scope): summary
chore(scope): summary
```

## 5. 代码标准

- 使用 C++17。
- 优先使用 RAII 和智能指针。
- 禁止使用原始指针表达所有权。
- 领域模块中避免隐藏式全局依赖。
- 异步流程必须有超时或 deadline。
- 状态机必须显式、可测试。

## 6. 文档标准

- 架构变化必须更新 ADR。
- 协议变化必须更新 `docs/PROTOCOL_DESIGN.md`。
- 模块边界变化必须更新 `docs/MODULE_DESIGN.md`。
- 安全敏感变更必须更新 `docs/SECURITY_DESIGN.md`。
- 可观测性变化必须更新 `docs/OBSERVABILITY.md`。
