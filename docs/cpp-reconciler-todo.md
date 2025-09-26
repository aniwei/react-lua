# C++ React Reconciler Roadmap

> 跟踪 C++ 版 React Reconciler 及配套工具链的实现进度。

## 核心 Reconciler
- [x] 搭建 `Packages/ReactReconcilerCpp` 目录结构与基础构建脚本
- [ ] 定义 Fiber 数据结构（节点字段、UpdateQueue、Lane 系统）
- [ ] 实现更新调度入口（`updateContainer`, `ensureRootIsScheduled` 等）
- [ ] 设计并实现 Hook 管理（state/effect queue）
  - [ ] 搭建 Effect 队列与 Hook 数据结构骨架
  - [ ] 搭建 state Hook 更新队列与 Hook 节点结构
  - [ ] 建立 Hook 渲染上下文（begin/update/finish 生命周期）
- [ ] 完成提交阶段逻辑（effect list、commit 入口）
- [ ] 引入错误边界与 Suspense 处理
- [ ] 编写核心单元测试（GoogleTest / Catch2）

## HostConfig / Scheduler 抽象
- [x] 定义跨平台 HostConfig 接口（创建、更新、提交、容器管理）
- [x] 定义 Scheduler 接口与优先级（Lane）交互契约
- [ ] 编写默认或 mock 实现用于测试
- [ ] 补充文档说明接口职责与扩展点

## HostInstance DOM 化
- [x] 设计 DOM 风格 `HostInstance` 结构（属性、样式、事件、树结构）
- [ ] 实现属性 diff／apply 工具
- [ ] 集成事件监听与生命周期钩子
- [ ] 添加针对 DOM Host 的测试用例

## WebAssembly 支持
- [ ] 迁移/编写 CMake + Emscripten 构建脚本生成 `.wasm/.js`
- [ ] 实现 Web HostConfig 与 Scheduler（浏览器 DOM / RAF）
- [ ] 封装 wasm loader（`loadReconcilerWasm()`）
- [ ] 在示例中验证 wasm 渲染流程
- [ ] 编写 `docs/wasm-integration.md`

## 浏览器端官方测试
- [ ] 选择并打包 React 官方单测子集
- [ ] 在浏览器 runner 中加载 wasm reconciler
- [ ] 打通 Vitest/Playwright 或 jest-lite 执行路径
- [ ] 集成 CI（GitHub Actions / bin/ci.sh）

## wasm 编译 & JS 调用（参考 `cheap`）
- [ ] 对齐 `cheap` 的 Emscripten 配置与目录布局
- [ ] 编写 JS wrapper（Promise 化加载、API 导出、类型声明）
- [ ] 创建示例 / smoke test 页面
- [ ] 设置 npm 包发布脚本

## 热更新开发工具
- [ ] 设计热更新签名机制（组件辨识、hooks 对齐）
- [ ] 与打包器集成（Vite/Rollup 插件）
- [ ] 在 wasm runtime 中实现补丁注入 API
- [ ] 扩展 DevTools 面板支持热更新状态展示
- [ ] 建立 end-to-end 热更新测试

## Hydration 支持
- [ ] 设计 Fiber Hydration 流程（挂载前树对齐、优先级策略）
- [ ] 扩展 HostConfig 提供 hydrate 专用 API（`hydrateInstance`, `hydrateTextInstance` 等）
- [ ] 实现选择性再渲染与差异回退逻辑
- [ ] 在浏览器/wasm 环境编写 hydration 集成测试

## 元数据与管理
- [ ] 更新 README，介绍 C++ Reconciler 与 wasm 支持愿景
- [ ] 建立贡献指南与代码规范（clang-format, clang-tidy）
- [ ] 规划性能基准并记录指标
