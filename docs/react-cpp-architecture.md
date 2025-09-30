# React C++ Runtime 技术方案

## 背景与目标

- 目标：在 C++ 中完整复刻 React 调和与并发框架，直接对应 `react-main/modules` 下的 React 源码结构。
- 统一数据结构：全部通过 `facebook::jsi` 访问 JS 值，不在 C++ 类中持有 `jsi::Runtime`；所有 API 都由调用方传入 `jsi::Runtime&`。
- 宿主：抽象出 `HostConfig`、`Scheduler`、`HostInstance` 接口；在浏览器场景下对应 DOM，实现 `HTMLElement` 风格 API，并继承 `jsi::HostObject`。
- 编译目标：支持 native 与 wasm，其中 wasm 使用 [aniwei/cheap](https://github.com/aniwei/cheap) 提供的高性能运行时；通过 wasm 运行官方 React JS 单测。

## 设计原则

1. **源文件映射**：C++ 文件命名、目录结构、函数流程对齐 React JS 文件（例如 `ReactFiberWorkLoop.js → fiber/ReactFiberWorkLoop.cpp`）。
2. **零 Runtime 持有**：任何类禁止保存 `jsi::Runtime` 指针或引用；每个函数显式传入 `jsi::Runtime&`。
3. **功能对等**：保留 React 的渲染阶段、并发调度、Hooks、Suspense、Hydration、错误恢复等全部逻辑。
4. **模块化宿主**：`HostConfig`/`Scheduler`/`HostInstance` 为纯虚接口，`ReactRuntime` 复用并聚合，实现面向不同平台的可插拔宿主。
5. **高性能桥接**：cheap/wasm 之间通过句柄和共享内存批量传输数据，避免重复拷贝 ReactElement/props。

## 架构分层

```
react-cpp/
├── runtime/           # ReactRuntime、RootRegistry、Lifecycle
├── interfaces/        # HostConfig.h, Scheduler.h, HostInstance.h 等
├── fiber/             # Fiber 定义、BeginWork、CompleteWork、Commit、Unwind
├── reconciler/        # ReactFiberReconciler、ChildReconciler、Hooks
├── scheduler/         # Scheduler 实现、TaskQueue、时间片
├── renderer/
│   ├── dom/           # DOMHostConfig、HTMLElementHostInstance、事件系统
│   └── server/        # SSR 支持
├── bridges/
│   ├── jsi/           # JsReference、RuntimeRegistry、HostObject glue
│   └── cheap/         # wasm 导入导出、数据格式
├── wasm/              # cheap CMake 配置、Emscripten 驱动
├── tests/             # 单测、集成测试、Playwright/Jest runner
└── docs/              # 方案文档、API 说明
```

## 核心组件

### ReactRuntime

- 继承 `HostConfig` & `Scheduler`，作为全局数据中心。
- 存储：`FiberRoot` 映射、调度状态、过渡跟踪、资源缓存等。
- API（参数均含 `jsi::Runtime&`）：
  - `createRoot(runtime, containerHandle, options)`
  - `hydrateRoot(runtime, containerHandle, elementRef, options)`
  - `render(runtime, rootHandle, elementRef)`
  - `dispatchEvent(runtime, rootHandle, eventType, eventHandle)`
  - `flushSync/flushPassiveEffects/runWithPriority` 等。

### HostConfig

- 参考 `ReactFiberConfig`，纯虚接口，主要方法：
  - `getRootHostContext(runtime, rootContainer)`
  - `createInstance(runtime, type, props, rootContainer, hostContext)`
  - `appendChild/appendInitialChild/insertBefore/removeChild`
  - `prepareUpdate/commitUpdate/resetAfterCommit`
  - `clearContainer/prepareForCommit/beforeActiveInstanceBlur`
  - Hydration：`tryHydrateInstance/prepareToHydrateHostInstance/resetHydrationState`
  - 资源/视图过渡：`startViewTransition/waitForCommitToBeReady`
- DOM 实现中所有 DOM 操作通过 cheap host function 调 JS。

### Scheduler

- 参考 `scheduler` 包，接口：
  ```cpp
  enum class SchedulerPriority { Immediate, UserBlocking, Normal, Low, Idle };
  using SchedulerCallback = std::function<SchedulerControl(jsi::Runtime&, bool didTimeout)>;
  virtual TaskHandle scheduleCallback(jsi::Runtime&, SchedulerPriority, SchedulerCallback) = 0;
  virtual void cancelCallback(jsi::Runtime&, TaskHandle) = 0;
  virtual double now(jsi::Runtime&) const = 0;
  virtual bool shouldYield(jsi::Runtime&) = 0;
  ```
- C++ Scheduler 负责与 cheap 调度队列交互，支持时间切片、帧限制、Idle 任务。

### HostInstance

- 继承 `jsi::HostObject`，接口仿 DOM。
- 方法示例：`appendChild(runtime, HostInstance& child)`, `setAttribute(runtime, name, value)`, `addEventListener(runtime, type, listener)`。
- DOM 实现保存 cheap `domNodeId` 句柄，diff props 时批量调用 cheap DOM API。

### Fiber 结构

- 与 JS 的 `FiberNode` 对齐：`tag`, `key`, `type`, `stateNode`, `child`, `sibling`, `returnFiber`, `alternate` 等。
- `Flags`, `Lanes`, `LanePriority`, `UpdateQueue`, `Dependencies` 全部复制 React JS 的字段。
- 创建/重置流程：`createFiber`, `createWorkInProgress`, `resetWorkInProgress`, `cloneChildFibers`。

## 调用与调度流程

1. **入口**：`ReactRuntime::render(runtime, root, reactElement)` → `updateContainer` → `scheduleUpdateOnFiber`。
2. **调度**：`ensureRootIsScheduled` 计算下一批 `lanes`，调用 `Scheduler::scheduleCallback` 注册任务。
3. **渲染阶段**：
   - `performConcurrentWorkOnRoot` → `renderRootConcurrent` → `workLoopConcurrent`。
   - `createWorkInProgress` 生成/复用 fiber；`beginWork` & `completeWork` 处理组件逻辑。
4. **Commit 阶段**：
   - `commitBeforeMutationEffects`：`prepareForCommit`，`clearContainer`，视图过渡。
   - `commitMutationEffects`：`Placement`, `Update`, `Deletion`, `Hydration`。
   - `commitLayoutEffects`、`commitPassiveMountEffects`。
5. **完成与后续**：
   - `finishConcurrentRender` 更新 `root.current`，处理 `pendingTransitionCallbacks`。
   - `flushPassiveEffects`、`schedulePendingInteractions`。

### createWorkInProgress 细节

```cpp
Fiber* createWorkInProgress(jsi::Runtime&, Fiber* current, Props pendingProps);
```
- 第一次渲染：分配新 Fiber，建立 `alternate`。
- 复用最小：清理 `flags`, `subtreeFlags`, `deletions`，复制 `memoizedProps`, `memoizedState`, `lanes` 等。
- `resetWorkInProgress` 在重入/中断时重置子树。

### Commit Before Mutation

- 遍历 `BeforeMutationMask`：
  - 处理 deletions → `commitBeforeMutationEffectsDeletion`
  - HostRoot 可能触发 `clearContainer`
  - Suspense/Offscreen 的视图过渡标记
- 完成后恢复 focus，重置视图过渡状态。

## 并发模式

- **Lanes**：比特掩码表示优先级，支持 `Sync`, `InputContinuous`, `Default`, `Transition`, `Retry`, `Offscreen` 等。
- `getNextLanes` 结合 `pendingLanes`, `suspendedLanes`, `pingedLanes`, `entangledLanes` 计算渲染顺序。
- **Scheduler**：时间切片由 `shouldYieldToHost` 决定；`scheduleCallback` 使用 cheap 的高精度计时器。
- **Transitions**：`useTransition`, `startTransition` 通过 `claimNextTransitionLane`; commit 后 `processTransitionCallbacks`。
- **Suspense**：`throwException` 标记 boundary，`attachPingListener` 监听 `wakeable`，`retryTimedOutBoundary` 重新调度。
- **Selective Hydration**：事件驱动 hydrate 目标子树，`queueHydrationTask` + retry lanes。
- **Offscreen & ViewTransition**：`OffscreenLane` 控制可见性；视图过渡 API 使用 `startViewTransition`。

## Hydration、错误与 unwind

- `hydrateRoot` 保留 SSR DOM，`root.isDehydrated = true`。
- `beginWork` 中 `tryToClaimNextHydratableInstance` 匹配已有节点；失败抛 `HydrationError` 强制客户端渲染。
- `commitHostHydrated*` 在 commit 阶段挂载复用节点。
- 错误流程：`handleThrow` 根据异常类型（Suspense、Hydration、普通错误）调用 `throwException`，生成 error update；无边界时触发 `onUncaughtError`。
- `unwindWork`/`unwindInterruptedWork` 清理 context、hooks、hydration 状态，保证后续重入。

## 浏览器 → cheap → wasm → C++ 链路

1. **JS API**：
   ```js
   const runtime = await createReactRuntime();
   const root = runtime.createRoot(container, {onRecoverableError});
   root.render(<App />);
   ```
2. **cheap**：
   - `createRuntime` 返回 `runtimeId`。
   - `createRef(value)` 生成 `jsRefId`。
   - `loadWasm` 引入 `react_runtime.wasm`，注入 DOM/Scheduler host 函数。
3. **wasm 导出**：`react_create_root`, `react_render`, `react_dispatch_event`, `react_flush_passive` 等，签名统一以 `runtimeId` 开头。
4. **C++**：`ReactRuntime` 调度 fiber workloop，通过 cheap DOM API 真正操作浏览器 DOM。

## wasm 与 cheap 数据格式

- **标识符**：`runtimeId`, `rootId`, `domNodeId`, `jsRefId`, `taskId`, `wakeableId` 等均为 `uint32_t`。
- **共享内存**：预留缓冲区传输大块数据（字符串、数组、diff payload），结构 `[len(int32)] + [bytes]`。
- **Host 函数表**（部分）：
  - `cheap_scheduler_schedule(runtimeId, priority, taskId)`
  - `cheap_dom_createElement(runtimeId, typeRefId, propsRefId)`
  - `cheap_dom_setAttribute(runtimeId, domNodeId, nameRefId, valueRefId)`
  - `cheap_js_createRef(runtimeId, value)` / `cheap_js_releaseRef`
  - `cheap_js_invokeFunction`, `cheap_promise_wrap`, `cheap_report_error`
- **C++ 数据结构**：
  - `RuntimeRegistry`/`RuntimeScope`：按需访问 `jsi::Runtime`。
  - `JsReference`：封装 `jsRefId`，提供 `deref`/`release`。
  - `DomHandle`：保存 `domNodeId`。
  - `SchedulerTask`、`WakeableHandle`、`HydrationToken`。
- **生命周期**：React fiber/host instance 释放时同步 `cheap.js.releaseRef`；DOM 删除时调 `cheap.dom.release`；任务取消时 `cheap_scheduler_cancel`。

## JSX 对象传递

1. JSX → Babel → `React.createElement` 生成 ReactElement。
2. `createRef` 包装为 `jsRefId`，调用 wasm `react_render`。
3. C++ `updateContainer` 将 `jsRefId` 作为 payload 入队；`beginWork` 懒访问 `props`、`type`、`children`。
4. 子元素 diff：`reconcileChildFibers` 遍历 `props.children`（`jsRefId` 数组），生成双方 `Fiber`。
5. commit 阶段 `diffProperties` 将新旧 `jsRefId` 传给 cheap DOM，实现精确更新。
6. 卸载后释放所有 `jsRefId`，避免 JS 内存泄漏。

## 实现阶段划分

1. **阶段 0：基础设施**
   - 建立目录结构、CMake/cheap toolchain、JSI glue。
   - 定义接口 (`HostConfig`, `Scheduler`, `HostInstance`, `ReactRuntime`) skeleton。
2. **阶段 1：Legacy 渲染**
   - Fiber + 调和主循环（`beginWork`, `completeWork`, `commitRoot`）。
   - DOM HostInstance 基础操作，验证首屏渲染。
3. **阶段 2：并发调度与 Hooks**
   - lanes、scheduler、`useState/useEffect`、`useTransition`。
   - 时间切片 & Suspense ping。
4. **阶段 3：Hydration & SSR**
   - HydrationContext、Selective Hydration、错误恢复。
5. **阶段 4：wasm 集成**
   - cheap 数据协议、浏览器事件桥接、Playwright 测试。
6. **阶段 5：全面测试与优化**
   - 对齐 React 官方单测、Benchmark、DevTools 支持。

## 测试与验证

- **单元测试**：Fiber 创建/复用、scheduler 队列、lanes 计算、Hook 状态机。
- **集成测试**：DOM 渲染、事件系统、Suspense、Transition、Hydration 场景。
- **wasm 浏览器测试**：使用 Playwright 跑 React 官方测试用例，实现 JS/wasm 结果比对。
- **静态检查**：clang-tidy/自定义规则，确保无类保存 `jsi::Runtime`。

## Requirements Coverage

- ✅ 数据结构全基于 `facebook::jsi`。
- ✅ `HostConfig` / `Scheduler` 抽象接口层设计。
- ✅ `HostInstance` DOM API + `jsi::HostObject`。
- ✅ `ReactRuntime` 继承 `HostConfig` & `Scheduler`，统一存储全局状态。
- ✅ wasm 方案结合 cheap，并运行官方 JS 单测。
- ✅ 文件命名/流程严格对齐 React 源码。
- ✅ 无任何类持有 `jsi::Runtime`，所有函数通过参数注入。
