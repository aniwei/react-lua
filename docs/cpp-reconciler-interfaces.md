# C++ Reconciler 接口说明

本文档概述 C++ 版 React Reconciler 的关键抽象层，帮助宿主环境或测试实现者了解各接口职责与扩展点。

## HostConfig

`HostConfig` 是宿主平台与 Reconciler 之间的桥梁，负责描述如何创建、更新与销毁宿主节点。

### 主要职责

- **上下文派发**：通过 `getRootHostContext` 和 `getChildHostContext` 为子树提供平台上下文（如 DOM 命名空间、渲染管线句柄等）。
- **节点创建**：`createInstance`/`createTextInstance` 将 Fiber 节点映射到宿主节点，通常返回宿主对象指针或智能指针。
- **初始挂载**：`appendInitialChild`、`finalizeInitialChildren` 负责在提交前构建初始子树并执行一次性副作用（如设置焦点）。
- **更新差异**：`prepareUpdate` 生成属性差异，`commitUpdate` 根据差异应用更新；文本节点使用 `commitTextUpdate`。
- **结构操作**：提供 `appendChild*`、`insert*`、`remove*` 等方法以维护宿主树结构。
- **资源回收**：`clearContainer` 与 `detachInstance` 处理卸载逻辑。
- **异步任务**：`scheduleMicrotask` 可将回调排入宿主的微任务队列，用于 effect 或后处理。

### 扩展建议

- 在浏览器宿主中，可将宿主实例包装为 `std::shared_ptr<HostNode>`，其中嵌入真实 DOM `emscripten::val`。
- 对于需要 hydration 的平台，后续会新增 `hydrateInstance`、`hydrateTextInstance` 等接口（参见 Roadmap 中的 Hydration 计划）。

## Scheduler

`Scheduler` 抽象了平台提供的调度与时间切片能力。

### 主要职责

- **任务调度**：`scheduleTask` 接收优先级（Lane）与工作函数，返回可取消的任务句柄；`cancelTask` 支持终止任务。
- **Host 回调**：`requestHostCallback` 允许 Reconciler 请求在宿主事件循环中执行一次性回调。
- **时间控制**：`now` 提供当前时间戳，`shouldYield` 与 `yieldUntilNextFrame` 帮助实现时间切片与让渡控制。

### 扩展建议

- 浏览器实现可绑定到 `requestIdleCallback` 或 `requestAnimationFrame`；游戏引擎可挂靠帧更新回调。
- 若宿主支持多线程，可在 `scheduleTask` 内部派发到任务队列，但需确保 Reconciler 主循环仍在单线程执行。

## HostInstance

`HostInstance` 描述宿主节点的 DOM 风格形态，主要用于 wasm/浏览器方向。

### 结构要点

- `NodeType` 区分元素节点、文本节点、注释节点或 Fragment。
- `attributes`、`style` 保存原始 `jsi::Value` 以便延迟解析；`eventListeners` 存储事件回调元数据。
- `children`、`parent` 构成宿主树；`layoutCache` 可缓存布局信息，供调试或性能分析。
- `platformPayload` 允许托管真实宿主句柄（如 DOM 节点、游戏对象等）。

### 扩展建议

- 可在宿主实现中为 `platformPayload` 自定义删除器或引用计数策略，避免跨语言资源泄漏。
- 若需支持调试工具，可扩展结构记录 `debugId`、`sourceLocation` 等信息。

## 测试工具

- `react_reconciler::testing::MockHostConfig` 提供纯内存宿主实现，适用于单元测试构建与 diff 验证。
- `react_reconciler::testing::MockScheduler` 允许控制任务执行顺序、模拟 `shouldYield` 行为。

## 后续扩展

- Hydration 增强：将在 HostConfig 中新增 hydrate API，并在 Fiber 流程中标记 hydrating flag。
- wasm/Web 支持：结合 `Lane` 优先级与 Scheduler，多平台实现需处理 requestAnimationFrame、微任务等差异。
- DevTools/热更新：依托 HostInstance 的结构化数据，可暴露 Fiber 树给调试器，并注入运行时热更新入口。
