# React C++ Scheduler TODO

> 最近一次更新：2025-10-02

## ✅ 已完成的迁移要点
- Root 调度状态统一存放在 `ReactRuntime::rootSchedulerState`，消除全局变量。
- `processRootSchedule` 接入 transition lanes，同步刷新走 `flushSyncWorkAcrossRoots`，与 JS 版保持一致。
- 新增 `requestTransitionLane`/`didCurrentEventScheduleTransition` C++ 实现，支持事件内的 lane 复用策略。
- 引入微任务去重逻辑：`ensureScheduleProcessing` 通过 `ensureScheduleIsScheduled` 安排一次“微任务”批处理。
- `flushSyncWorkAcrossRoots` 复用 C++ lane helpers，支持 legacy-only 刷新和 transition-triggered 强制同步。
- `ReactRuntime::shouldAttemptEagerTransition` 支持宿主回调，`processRootSchedule` 依赖该回调确定同步刷线路径。
- 默认过渡指示器调度逻辑：落地 `startDefaultTransitionIndicatorIfNeeded`、`retainIsomorphicIndicator`、`markIndicatorHandled`，支持在调度阶段启动并复用默认加载指示器。
- `entangleAsyncAction` 迁移至 runtime 持久状态，触发默认指示器并复用调度微任务保证检查时机。
- Commit 阶段补齐默认指示器清理：`performWorkOnRoot` 在完成同步 lanes 时调用 `markIndicatorHandled` 并执行 pending cleanup。
- 暴露 `registerRootDefaultIndicator` 入口，供 Root 创建流程注册默认指示器回调。

## 🚧 尚未完成的功能（共 5 项）
1. **默认过渡指示器后续**：仍需在 Root 创建流程里落地 `registerRootDefaultIndicator` 调用，并补齐 async action scope 结束时的 thenable/计数管理。
2. **`act` 专用微任务队列**：当前 `scheduleImmediateRootScheduleTask` 简化为即时执行，后续需接入测试环境的 `actQueue` 语义。
3. **被动 effect 让位策略**：需要迁移 `getRootWithPendingPassiveEffects`、`enableYieldingBeforePassive` 场景下的调度降级与恢复流程。
4. **Host Scheduler 对接**：`ReactRuntime::scheduleTask` 仍为占位实现，需落地与实际 host 调度器的优先级、取消、`shouldYield`、`now` 等行为。
5. **视图过渡与 async action 指示器**：仍需迁移视图过渡回调以及 async action scope 结束后的复位逻辑，完善默认指示器与视图过渡之间的交互。

## ⏭️ 下一步建议
- 继续推进 default indicator 及视图过渡相关逻辑，补齐与 transition 相关的剩余路径。
- 设计 host scheduler 适配层，提供微任务/宏任务抽象，解锁 `scheduleImmediateRootScheduleTask` 的真实异步行为。
- 在迁移 passive effects 前，补充与 JS 端一致的测试夹具，验证 `flushPendingEffects`、`hasPendingCommitEffects` 的时序。
