#pragma once

#include "../host/HostConfig.h"
#include "../host/Scheduler.h"
#include "../reconciler/FiberNode.h"
#include "jsi/jsi.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace jsi = facebook::jsi;

namespace react {

class UpdateQueue;

// ReactRuntime is the central hub of the C++ React core.
// It implements the HostConfig and Scheduler interfaces for a specific platform (e.g., Wasm/Browser).
class ReactRuntime : public HostConfig, public Scheduler {
public:
  ReactRuntime();
  ~ReactRuntime() override = default;

  // --- HostConfig Implementation ---
  std::shared_ptr<HostInstance> createInstance(
    jsi::Runtime& rt,
      const std::string& type,
    const jsi::Object& props
  ) override;
  
  std::shared_ptr<HostInstance> createTextInstance(
    jsi::Runtime& rt,
      const std::string& text
  ) override;

  void appendChild(
      std::shared_ptr<HostInstance> parent,
      std::shared_ptr<HostInstance> child
  ) override;
  
  void removeChild(
      std::shared_ptr<HostInstance> parent,
      std::shared_ptr<HostInstance> child
  ) override;

  void insertBefore(
      std::shared_ptr<HostInstance> parent,
      std::shared_ptr<HostInstance> child,
      std::shared_ptr<HostInstance> beforeChild) override;

  void commitUpdate(
    std::shared_ptr<HostInstance> instance,
    const jsi::Object& oldProps,
    const jsi::Object& newProps
  ) override;

  // --- Scheduler Implementation ---
  void scheduleImmediateTask(Task task) override;
  void scheduleNormalTask(Task task) override;

  // --- Global Entry Point ---
  // Renders a React element from its binary representation into a root container.
  void render(jsi::Runtime& rt, uint32_t rootElementOffset, std::shared_ptr<HostInstance> rootContainer);
  void hydrate(jsi::Runtime& rt, uint32_t rootElementOffset, std::shared_ptr<HostInstance> rootContainer);
  void reset();

private:
  struct ChildReconciliationResult {
    std::shared_ptr<FiberNode> firstChild;
    std::shared_ptr<FiberNode> lastPlacedChild;
    FiberFlags subtreeFlags{FiberFlags::NoFlags};
    std::vector<std::shared_ptr<FiberNode>> deletions;
  };

  std::shared_ptr<FiberNode> getOrCreateRoot(jsi::Runtime& rt, const std::shared_ptr<HostInstance>& rootContainer);
  void prepareFreshStack(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& root,
    const jsi::Value& element,
    bool isHydrating);
  void ensureRootScheduled(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& root);
  void performSyncWorkOnRoot(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& root);
  void commitRoot(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& finishedRoot);
  std::shared_ptr<FiberNode> performUnitOfWork(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& unit);
  std::shared_ptr<FiberNode> beginWorkPlaceholder(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& current,
    const std::shared_ptr<FiberNode>& workInProgress);
  void completeUnitOfWork(jsi::Runtime& rt, std::shared_ptr<FiberNode> unit);
  void completeWorkPlaceholder(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& current,
    const std::shared_ptr<FiberNode>& workInProgress);
  void reconcileChildrenPlaceholder(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& returnFiber,
    const std::shared_ptr<FiberNode>& currentFirstChild,
    const jsi::Value& newChild);
  void appendAllChildren(
    const std::shared_ptr<HostInstance>& parent,
    const std::shared_ptr<FiberNode>& child);
  std::shared_ptr<HostInstance> getHostParent(const std::shared_ptr<FiberNode>& fiber);
  void commitMutationEffects(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& root);
  void commitPlacement(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& fiber);
  void commitDeletion(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& parentFiber,
    const std::shared_ptr<FiberNode>& fiber);
  void collectHostChildren(
    const std::shared_ptr<FiberNode>& fiber,
    std::vector<std::shared_ptr<HostInstance>>& output);
  std::shared_ptr<FiberNode> createFiberFromElementPlaceholder(
    jsi::Runtime& rt,
    const jsi::Object& elementObject);
  std::shared_ptr<FiberNode> createFiberFromTextPlaceholder(
    jsi::Runtime& rt,
    const std::string& textContent);
  std::shared_ptr<FiberNode> reconcileSingleElement(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& returnFiber,
    const std::shared_ptr<FiberNode>& currentFirstChild,
    const jsi::Object& elementObject,
    ChildReconciliationResult& result);
  void deleteRemainingChildren(
    ChildReconciliationResult& result,
    const std::shared_ptr<FiberNode>& child);
  std::shared_ptr<FiberNode> placeChild(
    ChildReconciliationResult& result,
    const std::shared_ptr<FiberNode>& child,
    uint32_t index,
    bool shouldTrackMoves);
  std::shared_ptr<FiberNode> reconcileSingleText(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& returnFiber,
    const std::shared_ptr<FiberNode>& currentFirstChild,
    const std::string& textContent,
    ChildReconciliationResult& result);
  ChildReconciliationResult reconcileArrayChildren(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& returnFiber,
    const std::shared_ptr<FiberNode>& currentFirstChild,
    const jsi::Array& childrenArray);

  // Fiber tree globals
  std::shared_ptr<FiberNode> workInProgressRoot;
  std::shared_ptr<FiberNode> workInProgress;
  std::shared_ptr<FiberNode> currentRoot;
  std::shared_ptr<FiberNode> finishedWork;

  // Placeholder pointer to reconciler internals (to be defined later)
  void* reconciler_;

  std::unordered_map<HostInstance*, std::shared_ptr<FiberNode>> roots_;
  std::unordered_map<HostInstance*, std::shared_ptr<HostInstance>> rootContainers_;
  std::unordered_map<HostInstance*, bool> scheduledRoots_;
  std::unordered_map<FiberNode*, std::weak_ptr<HostInstance>> fiberHostContainers_;
};

} // namespace react
