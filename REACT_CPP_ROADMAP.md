# React-CPP: C++ 核心框架 - 高性能路线图

## 项目总览

**目标**: `react-lua` 为蓝本，使用 C++ 和 `facebook::jsi` 重写 React 核心。**JavaScript 与 C++ (Wasm) 之间的数据通信将通过预定义的二进制内存布局实现，以达到近似零拷贝的性能**。

**核心架构**:

1.  **Reconciler (C++)**: 实现 Fiber 架构，负责计算 UI 差异。
2.  **Scheduler (C++)**: 实现可中断的任务调度。
3.  **Host (宿主环境)**: 通过 `HostConfig` 和 `HostInstance` 接口与平台交互。
4.  **Runtime (C++ & JS)**:
    *   **ReactRuntime (C++)**: 项目的中心枢纽。
    *   **Wasm Bridge (JS & C++)**:
        *   **JS 侧**: 实现一个**内存布局引擎**，负责将 React Element 树按照 C++ 定义的结构，序列化到 Wasm 的线性内存中。
        *   **C++ 侧**: 实现一个**内存访问层**，能够直接从 Wasm 内存地址读取数据，并将其转换为 `jsi::Value` 供 Reconciler 使用。
5.  **JSI (C++)**: 作为 C++ 内部统一的数据表示层。

---

## TODO Roadmap (高性能内存布局版)

### 阶段 1: 项目基础与共享内存布局定义 (预计 2-3 周)

-   [x] **1.1. 目录结构与编译环境**:
    -   创建 `packages/ReactCpp` 目录。
    -   配置 CMake 和 `aniwei/cheap` 的 Wasm 工具链。

-   [x] **1.2. 依赖集成**:
    -   集成 `facebook::jsi` 头文件。

-   [x] **1.3. 核心 C++ 接口定义**:
    -   `src/host/HostInstance.h`
    -   `src/host/Scheduler.h`
    -   `src/host/HostConfig.h`
    -   `src/runtime/ReactRuntime.h`

-   [x] **1.4. 定义共享内存布局**:
    -   **关键任务**: 创建 `src/runtime/ReactWasmLayout.h`。
    -   使用 `#pragma pack(push, 1)` 来定义 C++/JS 共享的、无填充的二进制结构体。
    -   **定义**:
        -   `WasmReactValue`: 用于表示不同类型的值 (number, string, element, array) 的联合体 (union)。
        -   `WasmReactProp`: 表示一个 prop (key/value 对)。
        -   `WasmReactElement`: 核心结构，包含 `type`、`props` 数组指针、`children` 数组指针等。
    -   所有“指针”都定义为 `uint32_t`，表示相对于内存块基地址的偏移量。

### 阶段 2: Reconciler 核心与二进制桥接 (预计 3-4 周)

-   [x] **2.1. Fiber 节点与 UpdateQueue 定义**:
    -   `src/reconciler/FiberNode.h`: 定义 Fiber 属性集合、lanes、flags，与 `react-lua` 对齐。
    -   `src/reconciler/UpdateQueue.h`: 引入更新链表、队列初始化与入队工具函数。
    -   ✅ Fiber/UpdateQueue 结构已落地，等待与工作循环联动。

-   [ ] **2.2. Wasm 二进制桥接实现**:
    -   **C++ 侧**:
        -   [x] `ReactWasmLayout.h`: 新增 `WasmReactArray` 结构，统一数组布局。
        -   [x] `ReactWasmBridge.cpp`: 支持 Element/Array 反序列化、`key/ref/children` 映射，并返回 `jsi::Value`。
        -   [x] 导出 `react_hydrate`、容器注册/重置、JSI/Runtime 注入接口，形成可扩展的宿主桥接层。
    -   **JS 侧**:
        -   [x] `test/bridge.js`: 构建写入引擎 `writeElementTreeToWasmMemory(element)`，序列化字符串、布尔、数字、数组与嵌套 Element。

-   [ ] **2.3. 渲染入口实现**:
    -   [x] `ReactRuntime::render`：接受偏移量并调用反序列化，将结果提升为 `jsi::Value`。
    -   [x] Root Fiber 双缓冲结构初始化，`currentRoot`/`workInProgressRoot` 与宿主容器建立关联。
    -   [x] 为 Fiber Root 创建与调度提供真实实现，连通 HostInstance 映射。
    -   [ ] `ReactRuntime::hydrate`：偏移量入口已建立（占位实现），待补齐 hydration pipeline。

-   [ ] **2.4. 初步验证**:
    -   JS 调用 `render`，C++ 能接收到偏移量，并成功将二进制数据转换回 `jsi::Value`。

### 阶段 3: Reconciler 工作循环与 Diffing (预计 3-4 周)

-   [ ] **3.1. `beginWork`, `completeWork`, `workLoop` 实现**:
    -   这些函数的核心逻辑不变，因为它们操作的是已经从二进制格式解耦的 `FiberNode` 和 `jsi::Value`。
    -   [x] 搭建占位同步 `workLoop` 管线，串起 `beginWork`/`completeWork` 骨架。

-   [ ] **3.2. `reconcileChildren` (Diffing 算法) 实现**:
    -   严格参照 React 的 Diffing 逻辑，生成 `Placement`, `Update`, `Deletion` 等副作用。
    -   ⏳ 占位版 `reconcileChildrenPlaceholder` 已接入工作循环，覆盖单节点、文本与数组的 Fiber 构建与 Placement 记录。

-   [ ] **3.3. Commit 阶段实现**:
    -   实现 `commitRoot`，遍历副作用链表并调用 `HostConfig` 接口。
    -   ⏳ `commitMutationEffects`/`commitPlacement`/`commitDeletion` 占位逻辑已接线，可在宿主 stub 上模拟 `Placement` 与 `Deletion` 副作用。

-   [ ] **3.4. Hydration 支持**:
    -   在 `beginWork` 和 `completeWork` 中添加 `Hydrating` 模式。
    -   在此模式下，Reconciler 不会创建新的 `HostInstance` (即不调用 `createInstance`)，而是尝试从宿主环境获取现有的节点进行“附加”。
    -   如果客户端与服务器渲染的 DOM 不匹配，则需要进行错误恢复并切换回客户端渲染模式。

-   [ ] **3.5. 端到端验证**:
    -   **目标**: 渲染一个简单的、无状态的 JSX 结构 (`<div><p>Hello</p></div>`)。
    -   **目标**: 使用 `hydrate` 模式成功附加到一个预先存在的 DOM 结构上。
    -   需要编写专门的工具函数来打印 Wasm 内存区域的内容，以便于调试。

### 阶段 4: Scheduler 实现 (预计 1-2 周)

-   [ ] **4.1. 任务、优先级与任务队列定义**
-   [ ] **4.2. 调度循环与 `requestIdleCallback` 集成**
-   [ ] **4.3. 与 Reconciler 集成**

### 阶段 5: Hooks 实现 (预计 2-3 周)

-   [ ] **5.1. Hooks 数据结构与 Dispatcher 定义**
-   [ ] **5.2. `useState`, `useEffect` 实现**
-   [ ] **5.3. 验证带状态组件的更新和副作用**

### 阶段 6: 官方测试套件集成与迭代 (持续)

-   [ ] **6.1. 测试用例同步与 Jest 环境配置**
-   [ ] **6.2. 逐个通过测试**:
    -   **重点**: 这一阶段将极大地考验**二进制桥接的健壮性**。
    -   这是一个长期的、需要极大耐心和细心的打磨过程。
