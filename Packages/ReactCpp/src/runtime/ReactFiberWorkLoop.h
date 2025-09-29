#pragma once

#include "../react-dom/client/ReactDOMHostConfig.h"
#include "../react-dom/client/ReactDOMInstance.h"
#include "../scheduler/Scheduler.h"
#include "../reconciler/FiberNode.h"
#include "jsi/jsi.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace jsi = facebook::jsi;

namespace react {

class ReactFiberWorkLoopTestHelper;

class ReactFiberWorkLoop {
public:
  ReactFiberWorkLoop(ReactDOMHostConfig& hostConfig, Scheduler& scheduler);
  virtual ~ReactFiberWorkLoop() = default;

  // --- Global Entry Points ---
  void renderRootSync(jsi::Runtime& rt, uint32_t rootElementOffset, std::shared_ptr<ReactDOMInstance> rootContainer);
  void hydrateRoot(jsi::Runtime& rt, uint32_t rootElementOffset, std::shared_ptr<ReactDOMInstance> rootContainer);
  void resetWorkLoop();

  // --- Legacy Compatibility Shims ---
  void render(jsi::Runtime& rt, uint32_t rootElementOffset, std::shared_ptr<ReactDOMInstance> rootContainer);
  void hydrate(jsi::Runtime& rt, uint32_t rootElementOffset, std::shared_ptr<ReactDOMInstance> rootContainer);
  void reset();

private:
  friend class ReactFiberWorkLoopTestHelper;

  ReactDOMHostConfig& hostConfig_;
  Scheduler& scheduler_;

  struct ChildReconciliationResult {
    std::shared_ptr<FiberNode> firstChild;
    std::shared_ptr<FiberNode> lastPlacedChild;
    FiberFlags subtreeFlags{FiberFlags::NoFlags};
    uint32_t lastPlacedIndex{0};
    bool hasLastPlacedIndex{false};
    std::vector<std::shared_ptr<FiberNode>> deletions;
  };

  static std::string getTextContentFromValue(jsi::Runtime& rt, const jsi::Value& value);
  static bool computeHostComponentUpdatePayload(
    jsi::Runtime& rt,
    const jsi::Value& previousPropsValue,
    const jsi::Value& nextPropsValue,
    jsi::Value& outPayload);
  static bool computeHostTextUpdatePayload(
    jsi::Runtime& rt,
    const jsi::Value& previousPropsValue,
    const jsi::Value& nextPropsValue,
    jsi::Value& outPayload);

  std::shared_ptr<FiberNode> getOrCreateRoot(jsi::Runtime& rt, const std::shared_ptr<ReactDOMInstance>& rootContainer);
  void prepareFreshStack(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& root,
    const jsi::Value& element,
    bool isHydrating);
  void ensureRootScheduled(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& root);
  void performSyncWorkOnRoot(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& root);
  void commitRoot(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& finishedRoot);
  std::shared_ptr<FiberNode> performUnitOfWork(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& unit);
  std::shared_ptr<FiberNode> beginWork(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& current,
    const std::shared_ptr<FiberNode>& workInProgress);
  void completeUnitOfWork(jsi::Runtime& rt, std::shared_ptr<FiberNode> unit);
  void completeWork(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& current,
    const std::shared_ptr<FiberNode>& workInProgress);
  void appendAllChildren(
    const std::shared_ptr<ReactDOMInstance>& parent,
    const std::shared_ptr<FiberNode>& child);
  std::shared_ptr<ReactDOMInstance> getHostParent(const std::shared_ptr<FiberNode>& fiber);
  void collectHostChildren(
    const std::shared_ptr<FiberNode>& fiber,
    std::vector<std::shared_ptr<ReactDOMInstance>>& output);
  void commitPlacement(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& fiber);
  void commitDeletion(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& parentFiber,
    const std::shared_ptr<FiberNode>& fiber);
  void commitMutationEffects(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& root);
  void reconcileChildren(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& returnFiber,
    const std::shared_ptr<FiberNode>& currentFirstChild,
    const jsi::Value& newChild);
  std::shared_ptr<FiberNode> createFiberFromElement(
    jsi::Runtime& rt,
    const jsi::Object& elementObject);
  std::shared_ptr<FiberNode> createFiberFromText(
    jsi::Runtime& rt,
    const std::string& textContent);
  std::shared_ptr<FiberNode> cloneFiberForReuse(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& existing,
    jsi::Value pendingProps,
    jsi::Value keyCopy);
  std::shared_ptr<FiberNode> reconcileSingleElement(
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& returnFiber,
    const std::shared_ptr<FiberNode>& currentFirstChild,
    const jsi::Object& elementObject,
    ChildReconciliationResult& result);
  void deleteRemainingChildren(
    ChildReconciliationResult& result,
    const std::shared_ptr<FiberNode>& child);
  void markChildForDeletion(
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

  void runPendingTask(TaskHandle handle);

  // Fiber tree globals
  std::shared_ptr<FiberNode> workInProgressRoot;
  std::shared_ptr<FiberNode> workInProgress;
  std::shared_ptr<FiberNode> currentRoot;
  std::shared_ptr<FiberNode> finishedWork;

  void* reconciler_;

  std::unordered_map<ReactDOMInstance*, std::shared_ptr<FiberNode>> roots_;
  std::unordered_map<ReactDOMInstance*, std::shared_ptr<ReactDOMInstance>> rootContainers_;
  std::unordered_map<ReactDOMInstance*, bool> scheduledRoots_;
  std::unordered_map<FiberNode*, std::weak_ptr<ReactDOMInstance>> fiberHostContainers_;
};

class ReactFiberWorkLoopTestHelper {
public:
  static std::shared_ptr<FiberNode> cloneFiberForReuse(
    ReactFiberWorkLoop& runtime,
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& existing,
    jsi::Value pendingProps,
    jsi::Value keyCopy) {
    return runtime.cloneFiberForReuse(rt, existing, std::move(pendingProps), std::move(keyCopy));
  }

  static bool computeHostComponentUpdatePayload(
    jsi::Runtime& rt,
    const jsi::Value& previousPropsValue,
    const jsi::Value& nextPropsValue,
    jsi::Value& outPayload) {
    return ReactFiberWorkLoop::computeHostComponentUpdatePayload(rt, previousPropsValue, nextPropsValue, outPayload);
  }

  static bool computeHostTextUpdatePayload(
    jsi::Runtime& rt,
    const jsi::Value& previousPropsValue,
    const jsi::Value& nextPropsValue,
    jsi::Value& outPayload) {
    return ReactFiberWorkLoop::computeHostTextUpdatePayload(rt, previousPropsValue, nextPropsValue, outPayload);
  }

  static std::string getTextContentFromValue(jsi::Runtime& rt, const jsi::Value& value) {
    return ReactFiberWorkLoop::getTextContentFromValue(rt, value);
  }

  static void commitMutationEffects(
    ReactFiberWorkLoop& runtime,
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& root) {
    runtime.commitMutationEffects(rt, root);
  }

  static void reconcileChildren(
    ReactFiberWorkLoop& runtime,
    jsi::Runtime& rt,
    const std::shared_ptr<FiberNode>& returnFiber,
    const std::shared_ptr<FiberNode>& currentFirstChild,
    const jsi::Value& newChild) {
    runtime.reconcileChildren(rt, returnFiber, currentFirstChild, newChild);
  }

  static size_t getRegisteredRootCount(const ReactFiberWorkLoop& runtime) {
    return runtime.roots_.size();
  }
};

} // namespace react
