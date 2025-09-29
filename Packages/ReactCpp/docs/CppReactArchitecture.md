# C++ React 架构方案（对齐 react-main）

> 目标：以 `react-main` 分支（React 官方仓库主线）的 JavaScript 实现为蓝本，利用 `facebook::jsi` 重写 React 核心栈，构建可编译为 Wasm 的浏览器运行时，并支持官方 JS 测试套件。

## 1. 全局设计原则

- **JSI 统一数据接口**：所有在 C++ 层传递的动态数据（Fiber 节点字段、更新队列、Host 实例属性）均使用 `facebook::jsi` 提供的 `jsi::Value`、`jsi::Object`、`jsi::Array` 等类型，避免自定义动态类型系统，确保与 JS/Wasm 桥接的一致性。
- **模块映射**：对齐 `react-main` 仓库中的关键模块：
  | React main JavaScript 模块 | 核心职责 | C++ 对应实现 |
  | --- | --- | --- |
  | `packages/react-reconciler/src/ReactFiberWorkLoop.new.js` | Fiber 工作循环、begin/complete 阶段 | `react::ReactFiberWorkLoop::{beginWork, completeWork, performUnitOfWork}` |
  | `packages/react-reconciler/src/ReactFiberBeginWork.new.js` | 子 Fiber 构建与协调 | `react::ReactFiberWorkLoop::reconcileChildren*`, `FiberNode` 构造辅助 |
  | `packages/react-reconciler/src/ReactFiberCompleteWork.new.js` | 副作用生成、Host 节点管理 | `react::ReactFiberWorkLoop::completeWork*`, `commitMutationEffects` |
  | `packages/react-reconciler/src/ReactFiberHostConfig.js` | 宿主平台适配 | `react::ReactDOMHostConfig` 接口 + 平台实现 |
  | `packages/react-dom/src/client/ReactDOMHostConfig.js` | DOM 宿主实现 | `react::ReactDOMComponent` / `ReactDOMInstance` |
  | `packages/react/src/ReactFiberHooks.new.js` | Hooks 数据结构与调度 | C++ Hooks 模块（后续阶段） |
  | `packages/scheduler/src/*` | 时间切片与任务优先级 | `react::Scheduler` 接口 + 调度实现 |
  | `packages/shared/*` | 公共标志位、Lane 常量 | `react::FiberFlags`, `Lanes` 定义 |
  | `packages/react-dom/src/client/ReactDOMHostConfigWithNoop.js` | 测试环境 Host | `react::NoopReactDOMInstance`（测试桩） |

- **接口层分离**：`ReactDOMHostConfig` 与 `Scheduler` 必须保持纯虚接口，具体宿主实现（浏览器 DOM、原生 UI、测试桩）通过组合/继承注入。

- **接口层分离**：`ReactDOMHostConfig` 与 `Scheduler` 必须保持纯虚接口，具体宿主实现（浏览器 DOM、原生 UI、测试桩）通过组合/继承注入。
- **ReactFiberWorkLoop 作为中枢**：继承 `ReactDOMHostConfig` 与 `Scheduler`，维护 Fiber 树、任务队列、根容器注册表等全局状态；其他模块仅通过接口交互，避免外部持有全局单例。
- **Wasm 优先**：CMake 配置默认生成原生与 Wasm 双目标，Wasm 分发遵循 `aniwei/cheap` 的 toolchain 与运行时约定，浏览器端通过 Wasm 调用完成渲染并复用官方 JS 测试。

## 2. 核心模块与数据流

### 2.1 ReactFiberWorkLoop（继承 ReactDOMHostConfig + Scheduler）

职责：
- 根容器注册、调和循环、提交阶段、任务调度管理。
- 保持以下全局成员（均通过 `jsi` 表示动态数据）：
  - `workInProgressRoot` / `currentRoot` / `finishedWork` 双缓冲 Fiber 树。
  - `roots_`、`rootContainers_`、`scheduledRoots_`、`fiberHostContainers_` 等宿主映射。
  - `reconciler_` 指针预留给未来 Hooks/调试器扩展。
- 提供 `renderRootSync`/`hydrateRoot` API：
  1. 通过 `ReactWasmBridge` 从线性内存反序列化为 `jsi::Value`。
  2. 调用 `prepareFreshStack` 初始化 `workInProgressRoot`。
  3. 使用 `ensureRootScheduled` 驱动调和。
- `resetWorkLoop()` 支持测试环境下多次初始化。

内部分层：
- **Begin/Complete Work**：`beginWorkPlaceholder` / `completeWorkPlaceholder` 按 `react-main` 中 `ReactFiberWorkLoop` / `ReactFiberCompleteWork` 的语义复刻，保持 `WorkTag`、`FiberFlags` 与官方一致。
- **Commit 阶段**：`commitMutationEffects` 遍历副作用链，调用 `HostConfig` 接口操控宿主。
- **差异化扩展**：Hydration、事件系统、Context、Hooks 分阶段接入。

### 2.2 Fiber 与 UpdateQueue

- `FiberNode`：完整包含 React 官方字段（`tag`, `elementType`, `pendingProps`, `updateQueue`, `flags` 等），所有 prop/state 保存为 `jsi::Value`，确保二进制桥接到 JS 值时无需拷贝。
- `UpdateQueue`：对齐 `ReactFiberClassUpdateQueue.new.js` 定义的循环链表结构，`payload`/`reducer`/`callback` 经 `jsi::Value` 与 `std::function` 封装。
- Lane 模型：初期可维持简化数值型 `Lanes`，未来接入优先级细粒度管理。

### 2.3 ReactDOMHostConfig 接口层

```cpp
class ReactDOMHostConfig {
public:
  virtual std::shared_ptr<ReactDOMInstance> createInstance(jsi::Runtime&, const std::string& type, const jsi::Object& props) = 0;
  virtual std::shared_ptr<ReactDOMInstance> createTextInstance(jsi::Runtime&, const std::string& text) = 0;
  virtual void appendChild(std::shared_ptr<ReactDOMInstance> parent, std::shared_ptr<ReactDOMInstance> child) = 0;
  virtual void removeChild(std::shared_ptr<ReactDOMInstance> parent, std::shared_ptr<ReactDOMInstance> child) = 0;
  virtual void insertBefore(std::shared_ptr<ReactDOMInstance> parent, std::shared_ptr<ReactDOMInstance> child, std::shared_ptr<ReactDOMInstance> beforeChild) = 0;
  virtual void commitUpdate(std::shared_ptr<ReactDOMInstance> instance, const jsi::Object& oldProps, const jsi::Object& newProps, const jsi::Object& updatePayload) = 0;
};
```

- 提供与 `ReactDOMHostConfig` 等价的抽象，后续可增补 `prepareUpdate`、`getChildHostContext`、`finalizeInitialChildren` 等方法。
- ReactFiberWorkLoop 默认实现基于 `ReactDOMComponent`，浏览器/原生平台可替换派生类或桥接到真实 DOM。

### 2.4 Scheduler 接口层

- `scheduleTask(priority)` + `runWithPriority` + `shouldYield` 对齐 React Scheduler 优先级模型。
- 默认实现为同步执行，后续在 Wasm 环境下通过 `postMessage` + `requestIdleCallback` 衔接。
- 与 `packages/scheduler` 中的调度策略对齐：导入 `performConcurrentWorkOnRoot`、`scheduleCallback`、时间切片等逻辑，可按阶段迁移。

### 2.5 ReactDOMInstance 设计

- `ReactDOMInstance` 继承 `facebook::jsi::HostObject`，暴露 DOM 风格属性：
  - 只读：`tagName`, `children`。
  - 可写：`className`, `style`, `textContent`。
  - 方法：`appendChild`, `removeChild`, `insertChildBefore`, `setAttribute`, `removeAttribute`。
- `ReactDOMComponent` 持有 `runtime_` 指针，用 `jsi::Value` 缓存 props，便于 `commitUpdate` 时进行 diff。
- 对齐浏览器 `HTMLElement`：
  - `classList` 可延迟实现为 `HostObject`，初期提供 `className` 字符串。
  - 事件监听（如 `onclick`）保存为 `jsi::Function`，调和阶段更新并交由宿主事件系统触发。
- 继承 `enable_shared_from_this` 确保在 Append/Insert 时正确维护父子关系与智能指针生命周期。
- `diffReactDOMProperties` 函数直接参考 `react-main/packages/react-dom/src/client/ReactDOMComponent.js` 中的 `diffProperties` 实现：同样跳过 `children`、区分事件 (`onClick`)、`dangerouslySetInnerHTML` 与 `style` 将在后续阶段复刻，确保生成的 `payload` 与 JS 端结构保持一致。

### 2.6 全局调和流程

1. **JS/Wasm 侧** 调用：将 React Element 树序列化为 `WasmReactElement` 布局，调用 `react_render`。
2. **ReactFiberWorkLoop::renderRootSync**：反序列化 -> 准备 Fiber 栈 -> 调度。
3. **调和阶段**：
   - `beginWork`：根据 `WorkTag` 生成/复用子 Fiber。
   - `completeWork`：生成副作用（`Placement`, `Update`, `Deletion`）。
4. **提交阶段**：`commitMutationEffects` 遍历副作用链表，调用 `ReactDOMHostConfig` 接口操作 `ReactDOMInstance`。
5. **宿主层**：`ReactDOMComponent` 更新 DOM 状态或在真实浏览器中代理到 `HTMLElement`。

## 3. Wasm 编译与浏览器集成

### 3.1 构建管线

- 复用 `packages/ReactCpp/CMakeLists.txt`，引入 `cheap` 的工具链文件（`cheap/toolchain.cmake`）。
- 目标产物：
  - `libreact_cpp.a`（原生调试）。
  - `react_cpp.wasm` + `react_cpp.js`（Emscripten/cheap 生成）。
- 构建步骤：
  1. `cmake -B build/native -G Ninja` 构建原生单元测试。
  2. `cmake -B build/wasm -G Ninja -DCMAKE_TOOLCHAIN_FILE=...cheap-wasm.cmake` 生成 Wasm 文件。
  3. 输出 ABI：`react_init`, `react_render`, `react_hydrate`, `react_attach_jsi_runtime`, `react_attach_runtime`, `react_register_root_container`, `react_reset_runtime` 等入口。
- 内存导入：浏览器 JS 侧分配 ArrayBuffer 后传入 `react_init`，与 `ReactWasmLayout` 的结构体偏移保持一致。

### 3.2 浏览器端运行

- 运行时代码参考 `aniwei/cheap`：
  - 通过 WebAssembly.instantiate Streaming 加载 `.wasm`。
  - 将官方 React JS 代码（或 `react-lua` 的 JS 驱动脚本）与 Wasm 运行时绑定。
- `ReactDOMInstance` 在浏览器实现：包装原生 `HTMLElement`，通过 `JSI` 引用。
  - `createInstance` -> `document.createElement`。
  - `appendChild`/`removeChild` -> DOM API。
  - `commitUpdate` -> 属性、事件映射。
- Hydration：
  - 在 `hydration` 模式下，从既有 DOM 节点生成 `ReactDOMInstance`，并将其缓存到 Fiber `stateNode`。

### 3.3 官方测试接入

- 使用 Jest + jsdom + Wasm：
  - 在 Jest `setupFilesAfterEnv` 中加载 `react_cpp.wasm`。
  - 利用 `react_attach_jsi_runtime` 提供 JSI Runtime 接口（可采用 Hermes/V8 JSI stub）。
  - 将 React 官方单元测试中依赖的渲染 API 替换为 Wasm 版本（例如 `ReactDOM.render` -> 调用 `react_render`）。
- 测试执行：
  - 通过 Yarn/npm 脚本运行 Jest，确保 Wasm 模块在 Node 环境内可加载（`WebAssembly` 原生支持）。
  - 针对 DOM 依赖强的测试，可在 jsdom 中模拟 DOM，或使用 headless 浏览器运行。

## 4. 分阶段实施计划

| 阶段 | 目标 | 关键任务 | 输出 |
| --- | --- | --- | --- |
| 0. 基础完善 | 梳理接口、补全占位实现 | - 补齐 `ReactDOMInstance` DOM API<br>- 填充 `ReactFiberWorkLoop` 的 diff 与 commit 逻辑 stub<br>- 加强 `ReactDOMComponent` Props Diff | 单元测试覆盖 props 更新、child 操作 |
| 1. 调和循环 MVP | 支持静态树渲染 | - 完成 `reconcileChildren` 数组 diff<br>- `commitPlacement` / `commitDeletion` 打通<br>- JSON/JSI 属性 diff | `ReactFiberWorkLoopTests` 渲染 `<div><p>Hello</p></div>` 通过 |
| 2. Scheduler 集成 | 可中断更新 | - 定义任务优先级与时间切片<br>- 浏览器端使用 `postMessage`/`MessageChannel` 实现 | 通过模拟大量更新的基准测试 |
| 3. Hydration 与事件 | SSR 场景 + DOM 事件 | - `Hydrating` 模式 Fiber 管线<br>- DOM 事件监听器桥接 (`addEventListener`) | SSR + CSR 切换测试 |
| 4. Wasm 构建 | 浏览器运行 | - 整合 `cheap` toolchain<br>- JS Loader & Host 实现<br>- React 官方单测 smoke run | 在浏览器跑基础用例 |
| 5. 官方测试 | Jest 集成 | - 接入 React 官方测试 (逐步挑选模块)<br>- 修复差异 | `npm test -- react-official` 子集通过 |
| 6. 性能与工具链 | 优化 | - Profiler、DevTools Hook<br>- 性能基准（`bin/run-benchmarks.js`） | 性能报告 |

## 5. 技术细节与约束

- **JSI Runtime 可用性**：浏览器内可通过 Hermes/V8 JSI 接口或使用 QuickJS + 自定义 JSI adapter；Node 环境下可使用 Hermes CLI。
- **内存布局对齐**：`ReactWasmLayout` 使用 `#pragma pack(push, 1)`；JS 侧需用 `DataView`/`Uint8Array` 写入，避免对齐填充导致错读。
- **异常处理**：`ReactDOMInstance` 方法需向 JS 抛出 `jsi::JSError`；调和阶段最好捕获并标记 Fiber 进入错误边界（后续阶段实现）。
- **多线程**：初期运行在单线程（浏览器主线程）；后续可探索 Web Worker + `SharedArrayBuffer`。
- **测试策略**：
  - C++ 层使用 GoogleTest/`ReactFiberWorkLoopTests.cpp` 增量覆盖。
  - JS 层在 `test/bridge.js` 基础上扩展黑盒测试。

## 6. 开放问题与后续讨论

1. **react-main Hooks/事件模块映射**：梳理 `ReactFiberHooks`, `ReactDOMHostConfig`, `ReactDOMEventListener` 等 JS 模块的依赖，明确 C++ 需要同步的细分功能与初始化顺序。
2. **JSI Runtime 选择**：确定在 Node/Jest 环境下的 JSI runtime 实现（Hermes vs QuickJS）。
3. **Hydration 差异**：Lua 版若已实现 hydration，需要确认关键路径，确保 C++ 版行为一致。
4. **DevTools 支持**：后续是否需要兼容 React DevTools Bridge，决定是否提前在 Fiber 中保留 `debugID` 等字段。
5. **性能目标**：需制定基准指标（帧率、内存占用）以评估与 Lua/JS 实现的差距。

---

本方案以现有 `packages/ReactCpp` 代码骨架为起点，约束所有动态交互通过 `facebook::jsi` 完成，确保 Wasm 目标可无缝承载 React 官方测试；随阶段推进，逐步将 `react-main` 的核心特性迁移到 C++ 实现中。
