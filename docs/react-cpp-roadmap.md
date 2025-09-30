# React C++ Runtime Roadmap

> 本路线图基于《react-cpp-architecture》方案，按照阶段划分到函数级别的实现顺序。每个阶段包含目标、涉及模块、函数清单和验收标准。默认所有函数入参均显式接收 `jsi::Runtime&`。

## 阶段 0 · 基础设施与骨架

**目标**：搭建项目骨架、接口定义、工具链，确保能够编译空运行时并通过基础用例。

### 0.1 工程结构 & CMake
- `CMakeLists.txt`（根）
- `wasm/CMakeLists.txt`（cheap toolchain 适配）
- `packages/ReactCpp/include/react/ReactRuntime.h`
- `packages/ReactCpp/src/runtime/ReactRuntime.cpp`

### 0.2 接口定义
- `interfaces/HostConfig.h`
  - `virtual HostContext getRootHostContext(jsi::Runtime&, RootContainer const&) = 0;`
  - `virtual HostInstance* createInstance(jsi::Runtime&, jsi::String type, jsi::Object props, RootContainer const&, HostContext const&) = 0;`
  - `virtual void appendInitialChild(jsi::Runtime&, HostInstance&, HostInstance&) = 0;`
  - `virtual void appendChild(jsi::Runtime&, HostInstance&, HostInstance&) = 0;`
  - `virtual void insertBefore(jsi::Runtime&, HostInstance&, HostInstance&, HostInstance&) = 0;`
  - `virtual void removeChild(jsi::Runtime&, HostInstance&, HostInstance&) = 0;`
  - `virtual void prepareUpdate(jsi::Runtime&, HostInstance&, PropUpdatePayload&) = 0;`
  - `virtual void commitUpdate(jsi::Runtime&, HostInstance&, PropUpdatePayload const&) = 0;`
  - `virtual void prepareForCommit(jsi::Runtime&, ContainerHandle const&) = 0;`
  - `virtual void resetAfterCommit(jsi::Runtime&, ContainerHandle const&) = 0;`
  - `virtual void clearContainer(jsi::Runtime&, ContainerHandle const&) = 0;`
- `interfaces/Scheduler.h`
  - `virtual TaskHandle scheduleCallback(jsi::Runtime&, SchedulerPriority, SchedulerCallback) = 0;`
  - `virtual void cancelCallback(jsi::Runtime&, TaskHandle) = 0;`
  - `virtual double now(jsi::Runtime&) const = 0;`
  - `virtual bool shouldYield(jsi::Runtime&) = 0;`
- `interfaces/HostInstance.h`
  - `class HostInstance : public jsi::HostObject { ... }`
  - `virtual void appendChild(jsi::Runtime&, HostInstance&) = 0;`
  - `virtual void removeChild(jsi::Runtime&, HostInstance&) = 0;`
  - `virtual void insertBefore(jsi::Runtime&, HostInstance&, HostInstance&) = 0;`
  - `virtual void setAttribute(jsi::Runtime&, std::string_view name, jsi::Value const&) = 0;`
  - `virtual jsi::Value getAttribute(jsi::Runtime&, std::string_view name) const = 0;`
  - `virtual std::string_view tagName(jsi::Runtime&) const = 0;`
  - `virtual std::string className(jsi::Runtime&) const = 0;`
  - `virtual void addEventListener(jsi::Runtime&, std::string_view type, jsi::Function listener) = 0;`
  - `virtual void removeEventListener(jsi::Runtime&, std::string_view type, jsi::Function listener) = 0;`
- `bridges/jsi/RuntimeRegistry.h/.cpp`
  - `RuntimeScope RuntimeRegistry::acquire(uint32_t runtimeId);`
  - `jsi::Runtime& RuntimeScope::runtime() const;`

### 0.3 cheap/wasm 桥接基础
- `bridges/cheap/Exports.cpp`
  - `extern "C" RootHandle react_create_root(uint32_t runtimeId, uint32_t containerId, OptionsHandle opts);`
  - `extern "C" void react_render(uint32_t runtimeId, RootHandle root, JsRef elementRef);`
  - `extern "C" void react_dispatch_event(uint32_t runtimeId, RootHandle root, uint32_t eventTypeId, JsRef eventRef);`
  - `extern "C" void react_flush_passive(uint32_t runtimeId);`
- `bridges/cheap/HostFunctions.cpp`
  - `DomHandle cheap_dom_createElement(jsi::Runtime&, JsRef typeRef, JsRef propsRef);`
  - `void cheap_dom_appendChild(jsi::Runtime&, DomHandle parent, DomHandle child);`
  - `TaskHandle cheap_scheduler_schedule(jsi::Runtime&, SchedulerPriority, SchedulerCallback);`

**验收**：编译通过；可创建 root 并以空 HostConfig 无异常运行。

## 阶段 1 · Fiber 核心与同步渲染

**目标**：实现 Fiber 数据结构、同步渲染路径和基本 DOM Host 操作，支持简单组件树渲染。

### 1.1 Fiber 数据结构
- `fiber/Fiber.h`
  - `struct Fiber { ... }`
  - `Fiber* createFiber(FiberTag tag, JsReference type, std::string key, Mode mode);`
  - `Fiber* createWorkInProgress(jsi::Runtime&, Fiber* current, JsReference pendingProps);`
  - `void resetWorkInProgress(jsi::Runtime&, Fiber* workInProgress, Lanes renderLanes);`
  - `Fiber* cloneChildFibers(jsi::Runtime&, Fiber* current, Fiber* workInProgress);`
- `fiber/Lane.h`
  - `using Lane = uint32_t;`
  - `bool includesSyncLane(Lanes lanes);`
  - `Lanes mergeLanes(Lanes a, Lanes b);`
  - `Lanes getNextLanes(FiberRoot const*, Lanes wipLanes);`

### 1.2 UpdateQueue 与 Root
- `fiber/UpdateQueue.h`
  - `Update* createUpdate(Lane lane);`
  - `void enqueueUpdate(jsi::Runtime&, Fiber*, UpdateQueue*, Update*);`
  - `void initializeUpdateQueue(Fiber* fiber);`
  - `void processUpdateQueue(jsi::Runtime&, Fiber* workInProgress, UpdateQueue*, Props nextProps, jsi::Value* result);`
- `fiber/ReactFiberRoot.h/.cpp`
  - `FiberRoot* createFiberRoot(jsi::Runtime&, RootContainerHandle, RootTag, HydrationMode);`
  - `void markRootUpdated(FiberRoot*, Lane updateLane);`
  - `void markRootFinished(FiberRoot*, Lanes remainingLanes);`

### 1.3 Work Loop (同步版本)
- `runtime/ReactFiberWorkLoop.cpp`
  - `void scheduleUpdateOnFiber(jsi::Runtime&, Fiber*, Lane lane);`
  - `void ensureRootIsScheduled(jsi::Runtime&, FiberRoot*);`
  - `void performSyncWorkOnRoot(jsi::Runtime&, FiberRoot*);`
  - `void workLoopSync(jsi::Runtime&);`
  - `Fiber* performUnitOfWork(jsi::Runtime&, Fiber*);`
  - `Fiber* completeUnitOfWork(jsi::Runtime&, Fiber*);`

### 1.4 BeginWork & CompleteWork (最小集)
- `fiber/ReactFiberBeginWork.cpp`
  - `Fiber* beginWork(jsi::Runtime&, Fiber* current, Fiber* workInProgress, Lanes renderLanes);`
  - `Fiber* updateHostRoot(jsi::Runtime&, Fiber* current, Fiber* workInProgress, Lanes renderLanes);`
  - `Fiber* updateHostComponent(jsi::Runtime&, Fiber* current, Fiber* workInProgress, Lanes renderLanes);`
  - `Fiber* mountHostComponent(jsi::Runtime&, Fiber*, Lanes);`
- `fiber/ReactFiberCompleteWork.cpp`
  - `Fiber* completeWork(jsi::Runtime&, Fiber* current, Fiber* workInProgress, Lanes renderLanes);`
  - `void bubbleProperties(Fiber* completedWork);`

### 1.5 Commit (最小集)
- `fiber/ReactFiberCommitWork.cpp`
  - `void commitRoot(jsi::Runtime&, FiberRoot*, Fiber* finishedWork, Lanes committedLanes);`
  - `void commitBeforeMutationEffects(jsi::Runtime&, FiberRoot*, Fiber*, Lanes);`
  - `void commitMutationEffects(jsi::Runtime&, FiberRoot*, Fiber*, Lanes);`
  - `void commitLayoutEffects(jsi::Runtime&, FiberRoot*, Fiber*, Lanes);`
  - `void commitPlacement(jsi::Runtime&, Fiber*);`
  - `void commitDeletion(jsi::Runtime&, Fiber*, FiberRoot*);`

### 1.6 DOM HostConfig 最小实现
- `renderer/dom/DOMHostConfig.h/.cpp`
  - 实现接口：`createInstance`, `appendInitialChild`, `appendChild`, `removeChild`, `prepareUpdate`, `commitUpdate`, `clearContainer`, `prepareForCommit`, `resetAfterCommit`。
  - `HostInstance` 实现：`HTMLElementHostInstance`、`HostTextInstance`。

**验收**：使用简单 JSX（HostComponent + 文本）可在浏览器端渲染并更新。

## 阶段 2 · 并发调度与 Hooks

**目标**：完整实现 Scheduler、并发 work loop、Hooks 与 FunctionComponent 支持，达到生产级行为覆盖。

### 2.1 Scheduler 整合（完整实现）
- `scheduler/Scheduler.cpp`
  - `TaskHandle SchedulerImpl::scheduleCallback(jsi::Runtime&, SchedulerPriority, SchedulerCallback);`
  - `void SchedulerImpl::cancelCallback(jsi::Runtime&, TaskHandle);`
  - `double SchedulerImpl::now(jsi::Runtime&) const;`
  - `bool SchedulerImpl::shouldYield(jsi::Runtime&);`
- `runtime/ReactFiberWorkLoop.cpp`
  - 添加 `performConcurrentWorkOnRoot`
  - `renderRootConcurrent`
  - `workLoopConcurrent`
  - `shouldYieldToHost`
  - `requestEventTime`, `requestUpdateLane`

### 2.2 Lanes & Root Scheduler（完整实现）
- `fiber/ReactFiberLane.cpp`
  - `Lanes requestTransitionLane();`
  - `Lane claimNextTransitionLane();`
  - `void markRootSuspended(FiberRoot*, Lanes suspendedLanes);`
  - `void markRootPinged(FiberRoot*, Lanes pingedLanes);`
- `fiber/ReactFiberRootScheduler.cpp`
  - `void ensureRootIsScheduled(jsi::Runtime&, FiberRoot*, EventTime);`
  - `void performConcurrentWorkOnRoot(jsi::Runtime&, FiberRoot*);`
  - `void finishConcurrentRender(jsi::Runtime&, FiberRoot*, RootExitStatus, Lanes committedLanes);`

### 2.3 Hooks（完整实现）
- `reconciler/ReactFiberHooks.cpp`
  - `RenderLanes getCurrentRenderLanes();`
  - `void resetHooksAfterThrow();`
  - `Hook* mountWorkInProgressHook(jsi::Runtime&, Fiber*);`
  - `Hook* updateWorkInProgressHook(jsi::Runtime&, Fiber*);`
  - `jsi::Value useState(initialState);`
  - `jsi::Value useReducer(reducer, initialArg, init);`
  - `void useEffect(create, deps);`
  - `void useLayoutEffect(create, deps);`
  - `void useRef(initialValue);`
  - `void useMemo(factory, deps);`
- `reconciler/ReactFiberBeginWork.cpp`
  - `Fiber* updateFunctionComponent(jsi::Runtime&, Fiber* current, Fiber* workInProgress, Lanes renderLanes);`
  - `Fiber* mountIndeterminateComponent(jsi::Runtime&, Fiber*, Fiber*, Lanes);`

### 2.4 Passive Effects & Scheduler 集成（完整实现）
- `fiber/ReactFiberCommitWork.cpp`
  - `void commitPassiveMountEffects(jsi::Runtime&, FiberRoot*, Fiber* firstEffect);`
  - `void commitPassiveUnmountEffects(jsi::Runtime&, FiberRoot*, Fiber* firstEffect);`
  - `void flushPassiveEffects(jsi::Runtime&);`

**验收**：支持 FunctionComponent + Hooks；并发模式下 `startTransition`/`useTransition` 能正确调度；Passive effect 在帧后执行。

## 阶段 3 · Hydration & 高级特性

**目标**：实现 SSR Hydration、Selective Hydration、Suspense/Transitions 完整逻辑、错误处理。

### 3.1 Hydration
- `fiber/ReactFiberHydrationContext.cpp`
  - `void enterHydrationState(Fiber*, RootContainerHandle);`
  - `bool tryToClaimNextHydratableInstance(jsi::Runtime&, Fiber*);`
  - `void resetHydrationState(Fiber*);`
- `fiber/ReactFiberCompleteWork.cpp`
  - `void prepareToHydrateHostInstance(jsi::Runtime&, Fiber*, Fiber*, HostContext const&);`
  - `void prepareToHydrateHostTextInstance(jsi::Runtime&, Fiber*);`
- `fiber/ReactFiberCommitWork.cpp`
  - `void commitHydratedContainer(jsi::Runtime&, FiberRoot*);`
  - `void commitHydratedSuspense(jsi::Runtime&, Fiber*);`

### 3.2 Suspense & Error Handling
- `fiber/ReactFiberThrow.cpp`
  - `void throwException(jsi::Runtime&, Fiber* workInProgress, FiberRoot*, Fiber* returnFiber, jsi::Value thrownValue, Lanes renderLanes);`
  - `Update* createRootErrorUpdate(jsi::Runtime&, Fiber*, jsi::Value error, Lane lane);`
  - `void initializeClassErrorUpdate(Update* update, jsi::Value error, Lane lane);`
- `runtime/ReactFiberWorkLoop.cpp`
  - `void handleThrow(jsi::Runtime&, FiberRoot*, Fiber*, jsi::Value thrownValue);`
  - `void resetStackAfterSuspension();`
  - `void unwindInterruptedWork(jsi::Runtime&, Fiber*, Fiber*);`
- `fiber/ReactFiberCommitWork.cpp`
  - `void commitSuspenseyCommit(jsi::Runtime&, FiberRoot*, Fiber*);`
  - `void attachPingListener(jsi::Runtime&, Fiber*, FiberRoot*, WakeableHandle, Lane);`

### 3.3 Selective Hydration & Events
- `renderer/dom/HydrationEvents.cpp`
  - `void queueHydrationTask(jsi::Runtime&, FiberRoot*, Lane lane);`
  - `void runWithPriorityForDiscreteEvent(jsi::Runtime&, std::function<void(jsi::Runtime&)> callback);`
- `runtime/ReactEventProcessor.cpp`
  - `void dispatchEvent(jsi::Runtime&, RootHandle, EventTypeId, jsRef eventObject, DiscreteEventFlag);`
  - `void batchedEventUpdates(jsi::Runtime&, std::function<void(jsi::Runtime&)>);`

**验收**：
- SSR + `hydrateRoot` 成功复用 DOM。
- Suspense fallback、错误边界恢复、Transitions 生命周期完备。
- Hydration mismatch 报告 `onRecoverableError` 并可回退客户端渲染。

## 阶段 4 · wasm & cheap 集成

**目标**：连接浏览器、cheap、wasm 与 C++，实现事件、DOM、scheduler 的高性能互操作。

### 4.1 JS 包装器 (`scripts/translate-react.js` 扩展)
- 生成 C++ skeleton 所需的函数映射表。

### 4.2 cheap Host 函数
- `bridges/cheap/DomHost.cpp`
  - `DomHandle CheapDomHost::createElement(jsi::Runtime&, JsReference type, JsReference props);`
  - `void CheapDomHost::appendChild(jsi::Runtime&, DomHandle parent, DomHandle child);`
  - `void CheapDomHost::removeChild(jsi::Runtime&, DomHandle parent, DomHandle child);`
  - `void CheapDomHost::setAttribute(jsi::Runtime&, DomHandle node, JsReference name, JsReference value);`
- `bridges/cheap/SchedulerHost.cpp`
  - `TaskHandle CheapSchedulerHost::schedule(jsi::Runtime&, SchedulerPriority, SchedulerCallback);`
  - `void CheapSchedulerHost::cancel(jsi::Runtime&, TaskHandle);`
  - `double CheapSchedulerHost::now(jsi::Runtime&) const;`
  - `bool CheapSchedulerHost::shouldYield(jsi::Runtime&);`

### 4.3 wasm 导出/导入映射
- `bridges/cheap/Exports.cpp`
  - 扩展：`react_start_transition`, `react_flush_sync`, `react_report_fatal` 等。
- `wasm/bridge.js`
  - `createReactRuntime`, `createRoot`, `hydrateRoot`, `dispatchEvent` JS API。
  - 维护 `runtimeId`、`rootId`、`jsRefId`、`domNodeId` 映射，调用 wasm 导出函数。

### 4.4 Dev 工具
- `bridges/devtools/Inspector.cpp`
  - `void inspectFiber(jsi::Runtime&, Fiber*, SerializationBuffer&);`
  - `void enableDevToolsHook(jsi::Runtime&, DevToolsCallbacks const&);`

**验收**：
- 浏览器环境中通过 cheap 运行 wasm 版 React；与 JS 版比对渲染结果。
- 调度、事件、Hydration 在 wasm 模式下工作。
- 集成 Playwright/Jest 跑 React 官方测试子集。

## 阶段 5 · 完善与优化

**目标**：补齐剩余能力、性能优化、CI & 文档。

### 5.1 剩余 React 功能
- `fiber/ReactFiberCommitViewTransitions.cpp`
  - `void commitEnterViewTransitions(jsi::Runtime&, FiberRoot*, Fiber*);`
  - `void commitExitViewTransitions(jsi::Runtime&, FiberRoot*, Fiber*);`
- `fiber/ReactFiberCacheComponent.cpp`
  - `Cache* retainCache(Cache*);`
  - `void releaseCache(Cache*);`
- `fiber/ReactPostPaintCallback.cpp`
  - `void schedulePostPaintCallback(jsi::Runtime&, std::function<void(jsi::Runtime&)>);`

### 5.2 性能调优
- 批量 DOM 提交：`commitMutationEffects` 内合并操作，调用 `cheap_dom_batchMutations`。
- 字符串池：`StringPool::intern(jsi::Runtime&, JsReference strRef);`

### 5.3 测试 & CI
- `tests/runtime/*.cpp` 单元测试：lanes、hooks、scheduler。
- `tests/browser/playwright.config.ts` + `tests/browser/*.spec.ts`。
- GitHub Actions：native 构建、wasm 构建、JS 单测。

### 5.4 文档完善
- 更新 `docs/react-cpp-architecture.md`
- 编写 `docs/usage-browser.md`, `docs/usage-native.md`
- 生成 API reference (`doxygen` 或自定义文档)。

**验收**：CI 通过；性能与 React JS 对齐；文档完整。

---

## 附录 · 实施建议

- **代码生成**：扩展 `scripts/translate-react.js`，根据原 JS 文件生成 C++ 文件骨架，包含函数声明与 TODO 标记。
- **静态检查**：自定义 clang-tidy 规则确保无类持有 `jsi::Runtime`。
- **测试节奏**：每个阶段结束时运行相应单测/集成测试；早期引入 Playwright 便于验证 wasm 行为。
- **协同记录**：为每个函数编写 `TODO` 注释，链接对应 React JS 源文件行号，方便工程师对照实现。
