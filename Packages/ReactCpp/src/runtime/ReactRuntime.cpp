#include "ReactRuntime.h"
#include "ReactWasmLayout.h"
#include "jsi/jsi.h"
#include "../reconciler/UpdateQueue.h"
#include <cmath>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace jsi = facebook::jsi;

namespace react {

namespace {

inline bool hasFlag(FiberFlags value, FiberFlags flag) {
  return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

inline FiberFlags withoutFlag(FiberFlags value, FiberFlags flag) {
  return static_cast<FiberFlags>(static_cast<uint32_t>(value) & ~static_cast<uint32_t>(flag));
}

} // namespace

struct WasmReactValue;
jsi::Value convertWasmLayoutToJsi(
  jsi::Runtime& rt,
  uint32_t baseOffset,
  const WasmReactValue& wasmValue);

ReactRuntime::ReactRuntime()
  : workInProgressRoot(nullptr),
    workInProgress(nullptr),
    currentRoot(nullptr),
    finishedWork(nullptr),
    reconciler_(nullptr) {}

// --- HostConfig Implementation (Stubs) ---

std::shared_ptr<HostInstance> ReactRuntime::createInstance(
  jsi::Runtime& rt,
  const std::string& type,
  const jsi::Object& props) {
  // TODO: Call JS via Wasm import to create a real host instance.
  return nullptr;
}

std::shared_ptr<HostInstance> ReactRuntime::createTextInstance(
  jsi::Runtime& rt,
  const std::string& text) {
  // TODO: Call JS via Wasm import to create a real host text instance.
  return nullptr;
}

void ReactRuntime::appendChild(
    std::shared_ptr<HostInstance> parent,
    std::shared_ptr<HostInstance> child) {
  // TODO: Call JS via Wasm import.
}

void ReactRuntime::removeChild(
    std::shared_ptr<HostInstance> parent,
    std::shared_ptr<HostInstance> child) {
  // TODO: Call JS via Wasm import.
}

void ReactRuntime::insertBefore(
    std::shared_ptr<HostInstance> parent,
    std::shared_ptr<HostInstance> child,
    std::shared_ptr<HostInstance> beforeChild) {
  // TODO: Call JS via Wasm import.
}

void ReactRuntime::commitUpdate(
    std::shared_ptr<HostInstance> instance,
    const jsi::Object& oldProps,
    const jsi::Object& newProps) {
  // TODO: Call JS via Wasm import to update instance properties.
}

// --- Scheduler Implementation (Stubs) ---

void ReactRuntime::scheduleImmediateTask(Task task) {
  // For now, execute immediately.
  task();
}

void ReactRuntime::scheduleNormalTask(Task task) {
  // TODO: Call JS via Wasm import to use requestIdleCallback.
  // For now, execute immediately.
  task();
}

// --- Global Entry Points ---

void ReactRuntime::render(
  jsi::Runtime& rt,
  uint32_t rootElementOffset,
  std::shared_ptr<HostInstance> rootContainer) {
  if (!rootContainer) {
    return;
  }

  WasmReactValue rootElementValue{};
  rootElementValue.type = WasmValueType::Element;
  rootElementValue.data.ptrValue = rootElementOffset;

  jsi::Value element = convertWasmLayoutToJsi(rt, 0, rootElementValue);
  if (!element.isObject()) {
    return;
  }

  auto root = getOrCreateRoot(rt, rootContainer);
  prepareFreshStack(rt, root, element, /*isHydrating*/ false);

  ensureRootScheduled(rt, root);
}

void ReactRuntime::hydrate(
  jsi::Runtime& rt,
  uint32_t rootElementOffset,
  std::shared_ptr<HostInstance> rootContainer) {
  if (!rootContainer) {
    return;
  }

  WasmReactValue rootElementValue{};
  rootElementValue.type = WasmValueType::Element;
  rootElementValue.data.ptrValue = rootElementOffset;

  jsi::Value element = convertWasmLayoutToJsi(rt, 0, rootElementValue);
  if (!element.isObject()) {
    return;
  }

  auto root = getOrCreateRoot(rt, rootContainer);
  prepareFreshStack(rt, root, element, /*isHydrating*/ true);

  ensureRootScheduled(rt, root);
}

void ReactRuntime::reset() {
  workInProgressRoot.reset();
  workInProgress.reset();
  currentRoot.reset();
  finishedWork.reset();
  roots_.clear();
  rootContainers_.clear();
  scheduledRoots_.clear();
  fiberHostContainers_.clear();
}

std::shared_ptr<FiberNode> ReactRuntime::getOrCreateRoot(
  jsi::Runtime& rt,
  const std::shared_ptr<HostInstance>& rootContainer) {
  HostInstance* key = rootContainer.get();
  auto existing = roots_.find(key);
  if (existing != roots_.end()) {
    rootContainers_[key] = rootContainer;
    auto root = existing->second;
    if (!root->stateNode) {
      auto hostIt = fiberHostContainers_.find(root.get());
      if (hostIt != fiberHostContainers_.end()) {
        root->stateNode = hostIt->second.lock();
      }
    }
    if (!root->stateNode) {
      root->stateNode = rootContainer;
    }
    fiberHostContainers_[root.get()] = root->stateNode;
    return root;
  }

  auto root = std::make_shared<FiberNode>(WorkTag::HostRoot, jsi::Value::undefined(), jsi::Value::undefined());
  root->stateNode = rootContainer;
  root->pendingProps = jsi::Value::undefined();
  root->memoizedProps = jsi::Value::undefined();
  root->memoizedState = jsi::Value::undefined();

  roots_[key] = root;
  rootContainers_[key] = rootContainer;
  fiberHostContainers_[root.get()] = rootContainer;

  return root;
}

void ReactRuntime::prepareFreshStack(
  jsi::Runtime& rt,
  const std::shared_ptr<FiberNode>& root,
  const jsi::Value& element,
  bool isHydrating) {
  finishedWork = nullptr;
  currentRoot = root;

  // Ensure the root has an update queue for future scheduling.
  if (!root->updateQueue) {
    auto queue = std::make_shared<UpdateQueue>();
    queue->baseState = jsi::Value::undefined();
    root->updateQueue = queue;
  }

  workInProgressRoot = std::make_shared<FiberNode>(WorkTag::HostRoot, jsi::Value(rt, element), jsi::Value::undefined());
  workInProgressRoot->stateNode = root->stateNode;
  workInProgressRoot->alternate = root;
  root->alternate = workInProgressRoot;
  workInProgressRoot->updateQueue = root->updateQueue;
  workInProgressRoot->memoizedProps = jsi::Value::undefined();
  workInProgressRoot->memoizedState = jsi::Value::undefined();
  workInProgressRoot->flags = isHydrating ? FiberFlags::Hydrating : FiberFlags::NoFlags;
  workInProgressRoot->isHydrating = isHydrating;
  fiberHostContainers_[workInProgressRoot.get()] = workInProgressRoot->stateNode;

  root->pendingProps = jsi::Value(rt, element);

  workInProgress = workInProgressRoot;
}

void ReactRuntime::ensureRootScheduled(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& root) {
  if (!root) {
    return;
  }

  auto container = root->stateNode;
  if (!container) {
    return;
  }

  HostInstance* key = container.get();
  auto scheduledIt = scheduledRoots_.find(key);
  if (scheduledIt != scheduledRoots_.end() && scheduledIt->second) {
    return;
  }

  scheduledRoots_[key] = true;
  performSyncWorkOnRoot(rt, root);
  scheduledRoots_.erase(key);
}

void ReactRuntime::performSyncWorkOnRoot(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& root) {
  if (!root || !workInProgressRoot) {
    return;
  }

  workInProgress = workInProgressRoot;

  while (workInProgress) {
    auto next = performUnitOfWork(rt, workInProgress);
    if (next) {
      workInProgress = next;
      continue;
    }
    completeUnitOfWork(rt, workInProgress);
  }

  finishedWork = workInProgressRoot;
  workInProgress.reset();

  commitRoot(rt, finishedWork);
}

void ReactRuntime::commitRoot(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& finishedRoot) {
  if (!finishedRoot) {
    return;
  }

  finishedRoot->pendingState = jsi::Value::undefined();
  finishedRoot->pendingProps = jsi::Value::undefined();

  std::shared_ptr<HostInstance> container = finishedRoot->stateNode;
  HostInstance* key = container ? container.get() : nullptr;

  commitMutationEffects(rt, finishedRoot);

  auto previous = finishedRoot->alternate;
  if (previous) {
    previous->pendingProps = jsi::Value::undefined();
    previous->memoizedProps = jsi::Value(rt, finishedRoot->memoizedProps);
    if (!finishedRoot->memoizedState.isUndefined()) {
      previous->memoizedState = jsi::Value(rt, finishedRoot->memoizedState);
    } else {
      previous->memoizedState = jsi::Value::undefined();
    }
    previous->pendingState = jsi::Value::undefined();
    previous->child = finishedRoot->child;
    finishedRoot->alternate = previous;
    previous->alternate = finishedRoot;
    if (container) {
      fiberHostContainers_[previous.get()] = container;
    }
  }

  if (key) {
    roots_[key] = finishedRoot;
    fiberHostContainers_[finishedRoot.get()] = container;
  }

  currentRoot = finishedRoot;
  finishedWork.reset();
  workInProgressRoot.reset();
}

std::shared_ptr<FiberNode> ReactRuntime::performUnitOfWork(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& unit) {
  if (!unit) {
    return nullptr;
  }

  auto current = unit->alternate;
  auto next = beginWorkPlaceholder(rt, current, unit);
  if (next) {
    next->returnFiber = unit;
  }
  return next;
}

std::shared_ptr<FiberNode> ReactRuntime::beginWorkPlaceholder(
  jsi::Runtime& rt,
  const std::shared_ptr<FiberNode>& current,
  const std::shared_ptr<FiberNode>& workInProgress) {
  if (!workInProgress) {
    return nullptr;
  }

  workInProgress->flags = FiberFlags::NoFlags;
  workInProgress->subtreeFlags = FiberFlags::NoFlags;
  workInProgress->lanes = 0;
  workInProgress->childLanes = 0;

  switch (workInProgress->tag) {
    case WorkTag::HostRoot: {
      const jsi::Value& nextChildren = workInProgress->pendingProps;
      reconcileChildrenPlaceholder(rt, workInProgress, current ? current->child : nullptr, nextChildren);
      return workInProgress->child;
    }
    case WorkTag::HostComponent: {
      jsi::Value nextChildren = jsi::Value::undefined();
      if (!workInProgress->pendingProps.isUndefined() && workInProgress->pendingProps.isObject()) {
        jsi::Object props = workInProgress->pendingProps.asObject(rt);
        if (props.hasProperty(rt, "children")) {
          nextChildren = props.getProperty(rt, "children");
        }
      }
      reconcileChildrenPlaceholder(rt, workInProgress, current ? current->child : nullptr, nextChildren);
      return workInProgress->child;
    }
    case WorkTag::HostText:
    default:
      return nullptr;
  }
}

void ReactRuntime::completeUnitOfWork(jsi::Runtime& rt, std::shared_ptr<FiberNode> unit) {
  auto node = std::move(unit);
  while (node) {
    auto current = node->alternate;
    completeWorkPlaceholder(rt, current, node);

    auto sibling = node->sibling;
    if (sibling) {
      workInProgress = sibling;
      return;
    }

    node = node->returnFiber;
  }

  workInProgress.reset();
}

void ReactRuntime::completeWorkPlaceholder(
  jsi::Runtime& rt,
  const std::shared_ptr<FiberNode>& current,
  const std::shared_ptr<FiberNode>& workInProgress) {
  if (!workInProgress) {
    return;
  }

  switch (workInProgress->tag) {
    case WorkTag::HostRoot: {
      if (!workInProgress->pendingProps.isUndefined()) {
        workInProgress->memoizedProps = jsi::Value(rt, workInProgress->pendingProps);
      }
      workInProgress->pendingProps = jsi::Value::undefined();

      if (!workInProgress->pendingState.isUndefined()) {
        workInProgress->memoizedState = jsi::Value(rt, workInProgress->pendingState);
      }
      workInProgress->pendingState = jsi::Value::undefined();
      break;
    }
    case WorkTag::HostComponent: {
      if (current && current->stateNode) {
        workInProgress->stateNode = current->stateNode;
      }

      std::shared_ptr<HostInstance> instance = workInProgress->stateNode;
      if (!instance) {
        std::string typeName;
        if (workInProgress->type.isString()) {
          typeName = workInProgress->type.asString(rt).utf8(rt);
        } else if (workInProgress->elementType.isString()) {
          typeName = workInProgress->elementType.asString(rt).utf8(rt);
        }

        jsi::Object propsObject = workInProgress->pendingProps.isObject()
          ? workInProgress->pendingProps.asObject(rt)
          : jsi::Object(rt);

        instance = createInstance(rt, typeName, propsObject);
        if (instance) {
          appendAllChildren(instance, workInProgress->child);
        }
        workInProgress->stateNode = instance;
      }

      if (!workInProgress->pendingProps.isUndefined()) {
        workInProgress->memoizedProps = jsi::Value(rt, workInProgress->pendingProps);
        workInProgress->pendingProps = jsi::Value::undefined();
      }

      if (!workInProgress->pendingState.isUndefined()) {
        workInProgress->memoizedState = jsi::Value(rt, workInProgress->pendingState);
        workInProgress->pendingState = jsi::Value::undefined();
      }

      if (workInProgress->stateNode) {
        auto hostParent = getHostParent(workInProgress);
        if (hostParent) {
          fiberHostContainers_[workInProgress.get()] = hostParent;
        }
      }
      break;
    }
    case WorkTag::HostText: {
      if (current && current->stateNode) {
        workInProgress->stateNode = current->stateNode;
      }

      if (!workInProgress->stateNode) {
        std::string textContent;
        if (workInProgress->pendingProps.isString()) {
          textContent = workInProgress->pendingProps.asString(rt).utf8(rt);
        } else if (workInProgress->memoizedProps.isString()) {
          textContent = workInProgress->memoizedProps.asString(rt).utf8(rt);
        }
        workInProgress->stateNode = createTextInstance(rt, textContent);
      }

      if (!workInProgress->pendingProps.isUndefined()) {
        workInProgress->memoizedProps = jsi::Value(rt, workInProgress->pendingProps);
        workInProgress->pendingProps = jsi::Value::undefined();
      }
      if (!workInProgress->pendingState.isUndefined()) {
        workInProgress->memoizedState = jsi::Value(rt, workInProgress->pendingState);
        workInProgress->pendingState = jsi::Value::undefined();
      }

      if (workInProgress->stateNode) {
        auto hostParent = getHostParent(workInProgress);
        if (hostParent) {
          fiberHostContainers_[workInProgress.get()] = hostParent;
        }
      }
      break;
    }
    default: {
      if (!workInProgress->pendingProps.isUndefined()) {
        workInProgress->memoizedProps = jsi::Value(rt, workInProgress->pendingProps);
        workInProgress->pendingProps = jsi::Value::undefined();
      }
      if (!workInProgress->pendingState.isUndefined()) {
        workInProgress->memoizedState = jsi::Value(rt, workInProgress->pendingState);
        workInProgress->pendingState = jsi::Value::undefined();
      }
      break;
    }
  }

  FiberFlags aggregatedFlags = FiberFlags::NoFlags;
  auto child = workInProgress->child;
  while (child) {
    aggregatedFlags |= child->subtreeFlags;
    aggregatedFlags |= child->flags;
    child = child->sibling;
  }

  workInProgress->subtreeFlags = aggregatedFlags;
}

namespace {

std::string numberToText(double value) {
  if (std::isfinite(value)) {
    double truncated = std::trunc(value);
    if (truncated == value) {
      std::ostringstream oss;
      oss << static_cast<long long>(value);
      return oss.str();
    }
  }

  std::ostringstream oss;
  oss << value;
  return oss.str();
}

} // namespace

void ReactRuntime::appendAllChildren(
  const std::shared_ptr<HostInstance>& parent,
  const std::shared_ptr<FiberNode>& child) {
  if (!parent) {
    return;
  }

  auto node = child;
  while (node) {
    if (node->tag == WorkTag::HostComponent || node->tag == WorkTag::HostText) {
      if (node->stateNode) {
        appendChild(parent, node->stateNode);
        fiberHostContainers_[node.get()] = parent;
      }
    } else if (node->child) {
      appendAllChildren(parent, node->child);
    }
    node = node->sibling;
  }
}

std::shared_ptr<HostInstance> ReactRuntime::getHostParent(const std::shared_ptr<FiberNode>& fiber) {
  auto parent = fiber ? fiber->returnFiber : nullptr;
  while (parent) {
    if ((parent->tag == WorkTag::HostComponent || parent->tag == WorkTag::HostRoot) && parent->stateNode) {
      return parent->stateNode;
    }
    parent = parent->returnFiber;
  }
  return nullptr;
}

void ReactRuntime::collectHostChildren(
  const std::shared_ptr<FiberNode>& fiber,
  std::vector<std::shared_ptr<HostInstance>>& output) {
  auto node = fiber;
  while (node) {
    if (node->tag == WorkTag::HostComponent || node->tag == WorkTag::HostText) {
      if (node->stateNode) {
        output.push_back(node->stateNode);
      }
    } else if (node->child) {
      collectHostChildren(node->child, output);
    }
    node = node->sibling;
  }
}

void ReactRuntime::commitPlacement(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& fiber) {
  if (!fiber) {
    return;
  }

  std::shared_ptr<HostInstance> parentInstance;
  if (parentFiber) {
    if (parentFiber->tag == WorkTag::HostComponent || parentFiber->tag == WorkTag::HostRoot) {
      parentInstance = parentFiber->stateNode;
    } else {
      parentInstance = getHostParent(parentFiber);
    }
  } else {
    parentInstance = getHostParent(fiber);
  }

  if (!parentInstance) {
    auto mapping = fiberHostContainers_.find(fiber.get());
    if (mapping != fiberHostContainers_.end()) {
      parentInstance = mapping->second.lock();
    }
  }
  if (!parentInstance) {
    return;
  }

  auto existingContainer = fiberHostContainers_.find(fiber.get());
  if (existingContainer != fiberHostContainers_.end()) {
    if (auto locked = existingContainer->second.lock()) {
      if (locked.get() == parentInstance.get()) {
        return;
      }
    }
  }

  if (fiber->tag == WorkTag::HostComponent) {
    if (!fiber->stateNode) {
      std::string typeName;
      if (fiber->type.isString()) {
        typeName = fiber->type.asString(rt).utf8(rt);
      } else if (fiber->elementType.isString()) {
        typeName = fiber->elementType.asString(rt).utf8(rt);
      }

      jsi::Object propsObject = fiber->memoizedProps.isObject()
        ? fiber->memoizedProps.asObject(rt)
        : jsi::Object(rt);

      fiber->stateNode = createInstance(rt, typeName, propsObject);
      if (fiber->stateNode) {
        appendAllChildren(fiber->stateNode, fiber->child);
      }
    }

    if (fiber->stateNode) {
      if (existingContainer != fiberHostContainers_.end()) {
        if (auto locked = existingContainer->second.lock()) {
          if (locked && locked.get() != parentInstance.get()) {
            removeChild(locked, fiber->stateNode);
          }
        }
      }
      appendChild(parentInstance, fiber->stateNode);
      fiberHostContainers_[fiber.get()] = parentInstance;
    }
  } else if (fiber->tag == WorkTag::HostText) {
    if (!fiber->stateNode) {
      std::string textContent;
      if (fiber->memoizedProps.isString()) {
        textContent = fiber->memoizedProps.asString(rt).utf8(rt);
      } else if (fiber->pendingProps.isString()) {
        textContent = fiber->pendingProps.asString(rt).utf8(rt);
      }
      fiber->stateNode = createTextInstance(rt, textContent);
    }

    if (fiber->stateNode) {
      if (existingContainer != fiberHostContainers_.end()) {
        if (auto locked = existingContainer->second.lock()) {
          if (locked && locked.get() != parentInstance.get()) {
            removeChild(locked, fiber->stateNode);
          }
        }
      }
      appendChild(parentInstance, fiber->stateNode);
      fiberHostContainers_[fiber.get()] = parentInstance;
    }
  } else {
    auto child = fiber->child;
    while (child) {
      commitPlacement(rt, child);
      child = child->sibling;
    }
  }
}

void ReactRuntime::commitDeletion(
  jsi::Runtime& rt,
  const std::shared_ptr<FiberNode>& parentFiber,
  const std::shared_ptr<FiberNode>& fiber) {
  if (!fiber) {
    return;
  }

  std::shared_ptr<HostInstance> parentInstance;
  if (parentFiber) {
    if (parentFiber->tag == WorkTag::HostComponent || parentFiber->tag == WorkTag::HostRoot) {
      parentInstance = parentFiber->stateNode;
    } else {
      parentInstance = getHostParent(parentFiber);
    }
  } else {
    parentInstance = getHostParent(fiber);
  }

  if (!parentInstance) {
    auto mapping = fiberHostContainers_.find(fiber.get());
    if (mapping != fiberHostContainers_.end()) {
      parentInstance = mapping->second.lock();
    }
  }

  std::vector<std::shared_ptr<HostInstance>> nodesToRemove;
  collectHostChildren(fiber, nodesToRemove);

  for (auto& node : nodesToRemove) {
    if (parentInstance && node) {
      removeChild(parentInstance, node);
    }
  }

  std::vector<std::shared_ptr<FiberNode>> stack;
  stack.push_back(fiber);
  while (!stack.empty()) {
    auto currentFiber = stack.back();
    stack.pop_back();
    if (!currentFiber) {
      continue;
    }

    auto child = currentFiber->child;
    auto sibling = currentFiber->sibling;

    fiberHostContainers_.erase(currentFiber.get());
    currentFiber->flags = withoutFlag(currentFiber->flags, FiberFlags::Deletion);
    currentFiber->subtreeFlags = FiberFlags::NoFlags;
    currentFiber->deletions.clear();

    if (child) {
      stack.push_back(child);
    }
    if (sibling) {
      stack.push_back(sibling);
    }
    if (currentFiber->alternate) {
      currentFiber->alternate->alternate.reset();
      currentFiber->alternate.reset();
    }

    currentFiber->child.reset();
    currentFiber->sibling.reset();
    currentFiber->returnFiber.reset();
    currentFiber->stateNode.reset();
  }
}

void ReactRuntime::commitMutationEffects(jsi::Runtime& rt, const std::shared_ptr<FiberNode>& root) {
  if (!root) {
    return;
  }

  std::vector<std::shared_ptr<FiberNode>> stack;
  stack.push_back(root);

  while (!stack.empty()) {
    auto fiber = stack.back();
    stack.pop_back();
    if (!fiber) {
      continue;
    }

    if (!fiber->deletions.empty()) {
      for (auto& deletion : fiber->deletions) {
        commitDeletion(rt, fiber, deletion);
      }
      fiber->deletions.clear();
      fiber->flags = withoutFlag(fiber->flags, FiberFlags::ChildDeletion);
    }

    if (hasFlag(fiber->flags, FiberFlags::Deletion)) {
      commitDeletion(rt, fiber->returnFiber, fiber);
      continue;
    }

    if (hasFlag(fiber->flags, FiberFlags::Placement)) {
      commitPlacement(rt, fiber);
      fiber->flags = withoutFlag(fiber->flags, FiberFlags::Placement);
    }

    if (hasFlag(fiber->flags, FiberFlags::Update)) {
      // TODO: implement commitUpdate handling when diffing hooks are ready.
    }

    fiber->subtreeFlags = FiberFlags::NoFlags;

    std::vector<std::shared_ptr<FiberNode>> children;
    auto child = fiber->child;
    while (child) {
      children.push_back(child);
      child = child->sibling;
    }

    for (auto it = children.rbegin(); it != children.rend(); ++it) {
      stack.push_back(*it);
    }
  }
}

void ReactRuntime::reconcileChildrenPlaceholder(
  jsi::Runtime& rt,
  const std::shared_ptr<FiberNode>& returnFiber,
  const std::shared_ptr<FiberNode>& currentFirstChild,
  const jsi::Value& newChild) {
  if (!returnFiber) {
    return;
  }

  ReactRuntime::ChildReconciliationResult result;

  auto finalize = [&](const ReactRuntime::ChildReconciliationResult& reconciliation) {
    returnFiber->child = reconciliation.firstChild;
    returnFiber->subtreeFlags = reconciliation.subtreeFlags;
    if (!reconciliation.deletions.empty()) {
      returnFiber->deletions = reconciliation.deletions;
      returnFiber->flags |= FiberFlags::ChildDeletion;
      returnFiber->subtreeFlags |= FiberFlags::ChildDeletion;
    } else {
      returnFiber->deletions.clear();
      returnFiber->flags = withoutFlag(returnFiber->flags, FiberFlags::ChildDeletion);
    }
    if (returnFiber->child) {
      returnFiber->child->returnFiber = returnFiber;
      auto sibling = returnFiber->child->sibling;
      while (sibling) {
        sibling->returnFiber = returnFiber;
        sibling = sibling->sibling;
      }
    }
  };

  if (newChild.isUndefined() || newChild.isNull()) {
    deleteRemainingChildren(result, currentFirstChild);
    finalize(result);
    return;
  }

  if (newChild.isBool()) {
    if (!newChild.getBool()) {
      deleteRemainingChildren(result, currentFirstChild);
      finalize(result);
      return;
    }
    deleteRemainingChildren(result, currentFirstChild);
    finalize(result);
    return;
  }

  if (newChild.isString()) {
    std::string text = newChild.getString(rt).utf8(rt);
    (void)reconcileSingleText(rt, returnFiber, currentFirstChild, text, result);
    finalize(result);
    return;
  }

  if (newChild.isNumber()) {
    std::string text = numberToText(newChild.getNumber());
    (void)reconcileSingleText(rt, returnFiber, currentFirstChild, text, result);
    finalize(result);
    return;
  }

  if (newChild.isObject()) {
    jsi::Object obj = newChild.asObject(rt);
    if (obj.isArray(rt)) {
      auto arrayResult = reconcileArrayChildren(rt, returnFiber, currentFirstChild, obj.asArray(rt));
      finalize(arrayResult);
      return;
    }

    (void)reconcileSingleElement(rt, returnFiber, currentFirstChild, obj, result);
    finalize(result);
    return;
  }

  deleteRemainingChildren(result, currentFirstChild);
  finalize(result);
}

std::shared_ptr<FiberNode> ReactRuntime::createFiberFromElementPlaceholder(
  jsi::Runtime& rt,
  const jsi::Object& elementObject) {
  if (!elementObject.hasProperty(rt, "type")) {
    return nullptr;
  }

  jsi::Value typeValue = elementObject.getProperty(rt, "type");
  if (!typeValue.isString()) {
    return nullptr;
  }

  jsi::Value propsValue = elementObject.hasProperty(rt, "props")
    ? elementObject.getProperty(rt, "props")
    : jsi::Value::undefined();
  jsi::Value keyValue = elementObject.hasProperty(rt, "key")
    ? elementObject.getProperty(rt, "key")
    : jsi::Value::undefined();

  jsi::Value pendingPropsValue = propsValue.isUndefined()
    ? jsi::Value::undefined()
    : jsi::Value(rt, propsValue);
  jsi::Value keyCopy = keyValue.isUndefined()
    ? jsi::Value::undefined()
    : jsi::Value(rt, keyValue);

  auto fiber = std::make_shared<FiberNode>(
    WorkTag::HostComponent,
    pendingPropsValue,
    keyCopy);

  fiber->type = jsi::Value(rt, typeValue);
  fiber->elementType = jsi::Value(rt, typeValue);
  fiber->flags = FiberFlags::Placement;

  return fiber;
}

std::shared_ptr<FiberNode> ReactRuntime::createFiberFromTextPlaceholder(
  jsi::Runtime& rt,
  const std::string& textContent) {
  jsi::String text = jsi::String::createFromUtf8(rt, textContent);
  auto fiber = std::make_shared<FiberNode>(
    WorkTag::HostText,
    jsi::Value(text),
    jsi::Value::undefined());

  fiber->flags = FiberFlags::Placement;

  return fiber;
}

void ReactRuntime::deleteRemainingChildren(
  ReactRuntime::ChildReconciliationResult& result,
  const std::shared_ptr<FiberNode>& child) {
  auto node = child;
  while (node) {
    auto nextSibling = node->sibling;
    node->flags = node->flags | FiberFlags::Deletion;
    result.subtreeFlags |= FiberFlags::ChildDeletion;
    result.deletions.push_back(node);
    node = nextSibling;
  }
}

std::shared_ptr<FiberNode> ReactRuntime::placeChild(
  ReactRuntime::ChildReconciliationResult& result,
  const std::shared_ptr<FiberNode>& child,
  uint32_t index,
  bool shouldTrackMoves) {
  if (!child) {
    return nullptr;
  }

  auto nextSibling = child->sibling;
  child->sibling = nullptr;
  child->index = index;

  bool isNewFiber = child->alternate == nullptr;
  if (isNewFiber) {
    child->flags = child->flags | FiberFlags::Placement;
  } else if (shouldTrackMoves) {
    if (child->alternate && child->alternate->index > index) {
      child->flags = child->flags | FiberFlags::Placement;
    }
  }

  if (!result.firstChild) {
    result.firstChild = child;
  } else if (result.lastPlacedChild) {
    result.lastPlacedChild->sibling = child;
  }

  result.lastPlacedChild = child;
  result.subtreeFlags |= child->flags;
  result.subtreeFlags |= child->subtreeFlags;

  return nextSibling;
}

std::shared_ptr<FiberNode> ReactRuntime::reconcileSingleElement(
  jsi::Runtime& rt,
  const std::shared_ptr<FiberNode>& returnFiber,
  const std::shared_ptr<FiberNode>& currentFirstChild,
  const jsi::Object& elementObject,
  ReactRuntime::ChildReconciliationResult& result) {
  (void)returnFiber;

  jsi::Value typeValue = elementObject.getProperty(rt, "type");
  jsi::Value propsValue = elementObject.hasProperty(rt, "props")
    ? elementObject.getProperty(rt, "props")
    : jsi::Value::undefined();
  jsi::Value keyValue = elementObject.hasProperty(rt, "key")
    ? elementObject.getProperty(rt, "key")
    : jsi::Value::undefined();

  auto pendingProps = propsValue.isUndefined() ? jsi::Value::undefined() : jsi::Value(rt, propsValue);
  auto keyCopy = keyValue.isUndefined() ? jsi::Value::undefined() : jsi::Value(rt, keyValue);

  std::shared_ptr<FiberNode> existing = currentFirstChild;
  if (existing && existing->tag == WorkTag::HostComponent && typeValue.isString()) {
    bool typeMatches = existing->type.isString() && existing->type.asString(rt).utf8(rt) == typeValue.asString(rt).utf8(rt);
    bool keyMatches = true;
    if (!existing->key.isUndefined() || !keyValue.isUndefined()) {
      if (existing->key.isUndefined() || keyValue.isUndefined()) {
        keyMatches = false;
      } else {
        keyMatches = existing->key.asString(rt).utf8(rt) == keyValue.asString(rt).utf8(rt);
      }
    }

    if (typeMatches && keyMatches) {
      deleteRemainingChildren(result, existing->sibling);

      auto workInProgressFiber = std::make_shared<FiberNode>(
        existing->tag,
        pendingProps,
        keyCopy);

      workInProgressFiber->type = jsi::Value(rt, typeValue);
      workInProgressFiber->elementType = jsi::Value(rt, typeValue);
      workInProgressFiber->stateNode = existing->stateNode;
      workInProgressFiber->updateQueue = existing->updateQueue;
      workInProgressFiber->alternate = existing;
      existing->alternate = workInProgressFiber;
      workInProgressFiber->flags = FiberFlags::NoFlags;
      workInProgressFiber->subtreeFlags = FiberFlags::NoFlags;

      placeChild(result, workInProgressFiber, 0, false);
      return workInProgressFiber;
    }
  }

  deleteRemainingChildren(result, existing);
  auto fiber = createFiberFromElementPlaceholder(rt, elementObject);
  fiber->pendingProps = pendingProps;
  fiber->key = keyCopy;
  placeChild(result, fiber, 0, false);
  return fiber;
}

std::shared_ptr<FiberNode> ReactRuntime::reconcileSingleText(
  jsi::Runtime& rt,
  const std::shared_ptr<FiberNode>& returnFiber,
  const std::shared_ptr<FiberNode>& currentFirstChild,
  const std::string& textContent,
  ReactRuntime::ChildReconciliationResult& result) {
  (void)returnFiber;

  std::shared_ptr<FiberNode> existing = currentFirstChild;
  jsi::String textValue = jsi::String::createFromUtf8(rt, textContent);
  auto pendingProps = jsi::Value(textValue);

  if (existing && existing->tag == WorkTag::HostText) {
    deleteRemainingChildren(result, existing->sibling);

    auto workInProgressFiber = std::make_shared<FiberNode>(
      existing->tag,
      pendingProps,
      jsi::Value::undefined());

    workInProgressFiber->type = jsi::Value(rt, existing->type);
    workInProgressFiber->elementType = jsi::Value(rt, existing->elementType);
    workInProgressFiber->stateNode = existing->stateNode;
    workInProgressFiber->updateQueue = existing->updateQueue;
    workInProgressFiber->alternate = existing;
    existing->alternate = workInProgressFiber;
    workInProgressFiber->flags = FiberFlags::NoFlags;
    workInProgressFiber->subtreeFlags = FiberFlags::NoFlags;

    placeChild(result, workInProgressFiber, 0, false);
    return workInProgressFiber;
  }

  deleteRemainingChildren(result, existing);
  auto fiber = createFiberFromTextPlaceholder(rt, textContent);
  placeChild(result, fiber, 0, false);
  return fiber;
}

ReactRuntime::ChildReconciliationResult ReactRuntime::reconcileArrayChildren(
  jsi::Runtime& rt,
  const std::shared_ptr<FiberNode>& returnFiber,
  const std::shared_ptr<FiberNode>& currentFirstChild,
  const jsi::Array& childrenArray) {
  ReactRuntime::ChildReconciliationResult result;
  deleteRemainingChildren(result, currentFirstChild);

  size_t length = childrenArray.size(rt);
  uint32_t index = 0;
  for (size_t i = 0; i < length; ++i) {
    jsi::Value value = childrenArray.getValueAtIndex(rt, i);

    if (value.isUndefined() || value.isNull()) {
      continue;
    }

    if (value.isBool()) {
      if (!value.getBool()) {
        continue;
      }
      // true is ignored
      continue;
    }

    if (value.isString()) {
      auto fiber = createFiberFromTextPlaceholder(rt, value.getString(rt).utf8(rt));
      placeChild(result, fiber, index++, true);
      continue;
    }

    if (value.isNumber()) {
      auto fiber = createFiberFromTextPlaceholder(rt, numberToText(value.getNumber()));
      placeChild(result, fiber, index++, true);
      continue;
    }

    if (value.isObject()) {
      jsi::Object obj = value.asObject(rt);
      if (obj.isArray(rt)) {
        jsi::Array nested = obj.asArray(rt);
        size_t nestedLength = nested.size(rt);
        for (size_t j = 0; j < nestedLength; ++j) {
          jsi::Value nestedValue = nested.getValueAtIndex(rt, j);
          if (nestedValue.isString()) {
            auto fiber = createFiberFromTextPlaceholder(rt, nestedValue.getString(rt).utf8(rt));
            placeChild(result, fiber, index++, true);
          } else if (nestedValue.isNumber()) {
            auto fiber = createFiberFromTextPlaceholder(rt, numberToText(nestedValue.getNumber()));
            placeChild(result, fiber, index++, true);
          } else if (nestedValue.isObject()) {
            auto childFiber = createFiberFromElementPlaceholder(rt, nestedValue.asObject(rt));
            placeChild(result, childFiber, index++, true);
          }
        }
        continue;
      }

      auto fiber = createFiberFromElementPlaceholder(rt, obj);
      placeChild(result, fiber, index++, true);
      continue;
    }
  }

  return result;
}

} // namespace react
