#include "ReactRuntime.h"
#include "ReactWasmLayout.h"
#include "jsi/jsi.h"
#include "../host/SimpleHostInstance.h"
#include "../reconciler/UpdateQueue.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
  return std::make_shared<SimpleHostInstance>(rt, type, props);
}

std::shared_ptr<HostInstance> ReactRuntime::createTextInstance(
  jsi::Runtime& rt,
  const std::string& text) {
  return std::make_shared<SimpleHostInstance>(rt, "#text", text);
}

void ReactRuntime::appendChild(
    std::shared_ptr<HostInstance> parent,
    std::shared_ptr<HostInstance> child) {
  if (!parent || !child) {
    return;
  }

  if (auto simpleParent = std::dynamic_pointer_cast<SimpleHostInstance>(parent)) {
    simpleParent->appendChild(child);
  }
}

void ReactRuntime::removeChild(
    std::shared_ptr<HostInstance> parent,
    std::shared_ptr<HostInstance> child) {
  if (!parent || !child) {
    return;
  }

  if (auto simpleParent = std::dynamic_pointer_cast<SimpleHostInstance>(parent)) {
    simpleParent->removeChild(child);
  }
}

void ReactRuntime::insertBefore(
    std::shared_ptr<HostInstance> parent,
    std::shared_ptr<HostInstance> child,
    std::shared_ptr<HostInstance> beforeChild) {
  if (!parent || !child) {
    return;
  }

  if (auto simpleParent = std::dynamic_pointer_cast<SimpleHostInstance>(parent)) {
    simpleParent->insertChildBefore(child, beforeChild);
  }
}

void ReactRuntime::commitUpdate(
    std::shared_ptr<HostInstance> instance,
    const jsi::Object& oldProps,
    const jsi::Object& newProps,
    const jsi::Object& updatePayload) {
  if (!instance) {
    return;
  }

  if (auto simpleInstance = std::dynamic_pointer_cast<SimpleHostInstance>(instance)) {
    simpleInstance->applyUpdate(newProps, updatePayload);
  }
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

std::optional<std::string> extractKeyString(jsi::Runtime& rt, const jsi::Value& keyValue) {
  if (keyValue.isUndefined() || keyValue.isNull()) {
    return std::nullopt;
  }

  if (keyValue.isString()) {
    return keyValue.asString(rt).utf8(rt);
  }

  if (keyValue.isNumber()) {
    return numberToText(keyValue.getNumber());
  }

  return std::nullopt;
}

} // namespace

std::string ReactRuntime::getTextContentFromValue(jsi::Runtime& rt, const jsi::Value& value) {
  if (value.isString()) {
    return value.asString(rt).utf8(rt);
  }

  if (value.isNumber()) {
    return numberToText(value.getNumber());
  }

  if (value.isObject()) {
    jsi::Object object = value.asObject(rt);
    if (object.hasProperty(rt, "text")) {
      jsi::Value textValue = object.getProperty(rt, "text");
      if (textValue.isString()) {
        return textValue.asString(rt).utf8(rt);
      }
      if (textValue.isNumber()) {
        return numberToText(textValue.getNumber());
      }
    }
  }

  return std::string();
}

bool ReactRuntime::computeHostComponentUpdatePayload(
  jsi::Runtime& rt,
  const jsi::Value& previousPropsValue,
  const jsi::Value& nextPropsValue,
  jsi::Value& outPayload) {
  std::unordered_set<std::string> keys;

  auto collectKeys = [&](const jsi::Value& props) {
    if (!props.isObject()) {
      return;
    }
    jsi::Object object = props.asObject(rt);
    jsi::Array names = object.getPropertyNames(rt);
    size_t length = names.size(rt);
    for (size_t i = 0; i < length; ++i) {
      jsi::Value nameValue = names.getValueAtIndex(rt, i);
      if (!nameValue.isString()) {
        continue;
      }
      std::string key = nameValue.asString(rt).utf8(rt);
      if (key == "children") {
        continue;
      }
      keys.insert(std::move(key));
    }
  };

  collectKeys(previousPropsValue);
  collectKeys(nextPropsValue);

  if (keys.empty()) {
    bool previousIsObject = previousPropsValue.isObject();
    bool nextIsObject = nextPropsValue.isObject();
    if (previousIsObject == nextIsObject) {
      return false;
    }
    if (!previousIsObject && !nextIsObject) {
      return false;
    }
  }

  bool previousIsObject = previousPropsValue.isObject();
  bool nextIsObject = nextPropsValue.isObject();
  jsi::Object previousObject = previousIsObject ? previousPropsValue.asObject(rt) : jsi::Object(rt);
  jsi::Object nextObject = nextIsObject ? nextPropsValue.asObject(rt) : jsi::Object(rt);

  bool hasChanges = false;
  jsi::Object payload(rt);

  if (keys.empty()) {
    // There were no enumerable keys, but the object-ness changed (e.g., props removed).
    if (previousIsObject && !nextIsObject) {
      jsi::Array previousNames = previousObject.getPropertyNames(rt);
      size_t length = previousNames.size(rt);
      for (size_t i = 0; i < length; ++i) {
        jsi::Value nameValue = previousNames.getValueAtIndex(rt, i);
        if (!nameValue.isString()) {
          continue;
        }
        std::string key = nameValue.asString(rt).utf8(rt);
        if (key == "children") {
          continue;
        }
        auto nameID = jsi::PropNameID::forUtf8(rt, key);
        payload.setProperty(rt, nameID, jsi::Value::undefined());
        hasChanges = true;
      }
    }

    if (!previousIsObject && nextIsObject) {
      jsi::Array nextNames = nextObject.getPropertyNames(rt);
      size_t length = nextNames.size(rt);
      for (size_t i = 0; i < length; ++i) {
        jsi::Value nameValue = nextNames.getValueAtIndex(rt, i);
        if (!nameValue.isString()) {
          continue;
        }
        std::string key = nameValue.asString(rt).utf8(rt);
        if (key == "children") {
          continue;
        }
        auto nameID = jsi::PropNameID::forUtf8(rt, key);
        jsi::Value nextValue = nextObject.getProperty(rt, nameID);
        payload.setProperty(rt, nameID, jsi::Value(rt, nextValue));
        hasChanges = true;
      }
    }
  }

  for (const auto& key : keys) {
    auto nameID = jsi::PropNameID::forUtf8(rt, key);
    bool previousHas = previousIsObject && previousObject.hasProperty(rt, nameID);
    bool nextHas = nextIsObject && nextObject.hasProperty(rt, nameID);

    jsi::Value previousValue = previousHas ? previousObject.getProperty(rt, nameID) : jsi::Value::undefined();
    jsi::Value nextValue = nextHas ? nextObject.getProperty(rt, nameID) : jsi::Value::undefined();

    if (!nextHas) {
      if (previousHas) {
        payload.setProperty(rt, nameID, jsi::Value::undefined());
        hasChanges = true;
      }
      continue;
    }

    if (!previousHas || !jsi::Value::strictEquals(rt, previousValue, nextValue)) {
      payload.setProperty(rt, nameID, jsi::Value(rt, nextValue));
      hasChanges = true;
    }
  }

  if (!hasChanges) {
    return false;
  }

  outPayload = jsi::Value(rt, payload);
  return true;
}

bool ReactRuntime::computeHostTextUpdatePayload(
  jsi::Runtime& rt,
  const jsi::Value& previousPropsValue,
  const jsi::Value& nextPropsValue,
  jsi::Value& outPayload) {
  std::string previousText = getTextContentFromValue(rt, previousPropsValue);
  std::string nextText = getTextContentFromValue(rt, nextPropsValue);

  if (previousText == nextText) {
    return false;
  }

  jsi::Object payload(rt);
  payload.setProperty(rt, "text", jsi::String::createFromUtf8(rt, nextText));
  outPayload = jsi::Value(rt, payload);
  return true;
}

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
  auto parentFiber = fiber->returnFiber;
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
    currentFiber->updatePayload = jsi::Value::undefined();

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
      switch (fiber->tag) {
        case WorkTag::HostComponent: {
          auto instance = fiber->stateNode;
          if (instance) {
            jsi::Object oldProps = fiber->alternate && fiber->alternate->memoizedProps.isObject()
              ? fiber->alternate->memoizedProps.asObject(rt)
              : jsi::Object(rt);
            jsi::Object newProps = fiber->memoizedProps.isObject()
              ? fiber->memoizedProps.asObject(rt)
              : jsi::Object(rt);

            jsi::Object payload = fiber->updatePayload.isObject()
              ? fiber->updatePayload.asObject(rt)
              : jsi::Object(rt);

            bool hasPayload = false;
            if (fiber->updatePayload.isObject()) {
              jsi::Array payloadKeys = payload.getPropertyNames(rt);
              hasPayload = payloadKeys.size(rt) > 0;
            }

            if (hasPayload) {
              commitUpdate(instance, oldProps, newProps, payload);
            }
          }
          fiber->updatePayload = jsi::Value::undefined();
          fiber->flags = withoutFlag(fiber->flags, FiberFlags::Update);
          break;
        }
        case WorkTag::HostText: {
          auto instance = fiber->stateNode;
          if (instance) {
            std::string newText = getTextContentFromValue(rt, fiber->memoizedProps);
            std::string oldText = fiber->alternate
              ? getTextContentFromValue(rt, fiber->alternate->memoizedProps)
              : std::string();

            jsi::Object oldProps(rt);
            jsi::Object newProps(rt);
            oldProps.setProperty(rt, "text", jsi::String::createFromUtf8(rt, oldText));
            newProps.setProperty(rt, "text", jsi::String::createFromUtf8(rt, newText));

            jsi::Object payload = fiber->updatePayload.isObject()
              ? fiber->updatePayload.asObject(rt)
              : jsi::Object(rt);

            bool hasPayload = false;
            if (fiber->updatePayload.isObject()) {
              jsi::Array payloadKeys = payload.getPropertyNames(rt);
              hasPayload = payloadKeys.size(rt) > 0;
            }

            if (!hasPayload && newText != oldText) {
              payload.setProperty(rt, "text", jsi::String::createFromUtf8(rt, newText));
              hasPayload = true;
            }

            if (hasPayload) {
              commitUpdate(instance, oldProps, newProps, payload);
            }
          }
          fiber->updatePayload = jsi::Value::undefined();
          fiber->flags = withoutFlag(fiber->flags, FiberFlags::Update);
          break;
        }
        default:
          fiber->updatePayload = jsi::Value::undefined();
          fiber->flags = withoutFlag(fiber->flags, FiberFlags::Update);
          break;
      }
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
    std::move(pendingPropsValue),
    std::move(keyCopy));

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
    jsi::Value(rt, text),
    jsi::Value::undefined());

  fiber->flags = FiberFlags::Placement;

  return fiber;
}

std::shared_ptr<FiberNode> ReactRuntime::cloneFiberForReuse(
  jsi::Runtime& rt,
  const std::shared_ptr<FiberNode>& existing,
  jsi::Value pendingProps,
  jsi::Value keyCopy) {
  if (!existing) {
    return nullptr;
  }

  jsi::Value updatePayload = jsi::Value::undefined();
  bool shouldMarkUpdate = false;
  if (existing->tag == WorkTag::HostComponent) {
    shouldMarkUpdate = computeHostComponentUpdatePayload(rt, existing->memoizedProps, pendingProps, updatePayload);
  } else if (existing->tag == WorkTag::HostText) {
    shouldMarkUpdate = computeHostTextUpdatePayload(rt, existing->memoizedProps, pendingProps, updatePayload);
  } else {
    const auto& previousProps = existing->memoizedProps;
    bool pendingUndefined = pendingProps.isUndefined();
    bool previousUndefined = previousProps.isUndefined();
    if (pendingUndefined != previousUndefined) {
      shouldMarkUpdate = true;
    } else if (!pendingUndefined) {
      shouldMarkUpdate = !jsi::Value::strictEquals(rt, pendingProps, previousProps);
    }
  }

  auto fiber = std::make_shared<FiberNode>(
    existing->tag,
    std::move(pendingProps),
    std::move(keyCopy));

  fiber->type = existing->type.isUndefined()
    ? jsi::Value::undefined()
    : jsi::Value(rt, existing->type);
  fiber->elementType = existing->elementType.isUndefined()
    ? jsi::Value::undefined()
    : jsi::Value(rt, existing->elementType);
  fiber->memoizedProps = existing->memoizedProps.isUndefined()
    ? jsi::Value::undefined()
    : jsi::Value(rt, existing->memoizedProps);
  fiber->memoizedState = existing->memoizedState.isUndefined()
    ? jsi::Value::undefined()
    : jsi::Value(rt, existing->memoizedState);
  fiber->stateNode = existing->stateNode;
  fiber->updateQueue = existing->updateQueue;
  fiber->flags = FiberFlags::NoFlags;
  fiber->subtreeFlags = FiberFlags::NoFlags;

  if (shouldMarkUpdate) {
    fiber->flags = fiber->flags | FiberFlags::Update;
    fiber->updatePayload = std::move(updatePayload);
  }

  fiber->alternate = existing;
  existing->alternate = fiber;
  existing->updatePayload = jsi::Value::undefined();

  return fiber;
}

void ReactRuntime::deleteRemainingChildren(
  ReactRuntime::ChildReconciliationResult& result,
  const std::shared_ptr<FiberNode>& child) {
  auto node = child;
  while (node) {
    auto nextSibling = node->sibling;
    markChildForDeletion(result, node);
    node = nextSibling;
  }
}

void ReactRuntime::markChildForDeletion(
  ReactRuntime::ChildReconciliationResult& result,
  const std::shared_ptr<FiberNode>& child) {
  if (!child) {
    return;
  }

  if (!hasFlag(child->flags, FiberFlags::Deletion)) {
    child->flags = child->flags | FiberFlags::Deletion;
  }

  auto already = std::find_if(
    result.deletions.begin(),
    result.deletions.end(),
    [&](const std::shared_ptr<FiberNode>& existing) {
      return existing.get() == child.get();
    });
  if (already != result.deletions.end()) {
    return;
  }

  result.subtreeFlags |= FiberFlags::ChildDeletion;
  result.deletions.push_back(child);
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
    if (shouldTrackMoves) {
      child->flags = child->flags | FiberFlags::Placement;
    }
  } else if (shouldTrackMoves) {
    uint32_t oldIndex = child->alternate->index;
    if (!result.hasLastPlacedIndex || oldIndex >= result.lastPlacedIndex) {
      result.lastPlacedIndex = oldIndex;
      result.hasLastPlacedIndex = true;
    } else {
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
      auto workInProgressFiber = cloneFiberForReuse(rt, existing, std::move(pendingProps), std::move(keyCopy));
      placeChild(result, workInProgressFiber, 0, false);
      return workInProgressFiber;
    }
  }

  deleteRemainingChildren(result, existing);
  auto fiber = createFiberFromElementPlaceholder(rt, elementObject);
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
  auto pendingProps = jsi::Value(rt, textValue);

  if (existing && existing->tag == WorkTag::HostText) {
    deleteRemainingChildren(result, existing->sibling);
    auto workInProgressFiber = cloneFiberForReuse(rt, existing, std::move(pendingProps), jsi::Value::undefined());
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
  (void)returnFiber;

  ReactRuntime::ChildReconciliationResult result;

  struct ExistingEntry {
    std::shared_ptr<FiberNode> fiber;
    std::optional<std::string> key;
  };

  std::vector<ExistingEntry> existingEntries;
  std::unordered_map<std::string, std::shared_ptr<FiberNode>> keyedExisting;

  auto existing = currentFirstChild;
  while (existing) {
    auto keyOpt = extractKeyString(rt, existing->key);
    existingEntries.push_back({existing, keyOpt});
    if (keyOpt) {
      keyedExisting[*keyOpt] = existing;
    }
    existing = existing->sibling;
  }

  std::unordered_set<FiberNode*> reused;

  auto takeFirstUnkeyed = [&](WorkTag tag, const std::function<bool(const std::shared_ptr<FiberNode>&)>& predicate)
    -> std::shared_ptr<FiberNode> {
    for (auto& entry : existingEntries) {
      const auto& candidate = entry.fiber;
      if (!candidate) {
        continue;
      }
      if (reused.find(candidate.get()) != reused.end()) {
        continue;
      }
      if (entry.key.has_value()) {
        continue;
      }
      if (candidate->tag != tag) {
        continue;
      }
      if (predicate && !predicate(candidate)) {
        continue;
      }
      reused.insert(candidate.get());
      return candidate;
    }
    return nullptr;
  };

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
      continue;
    }

    if (value.isString()) {
      std::string text = value.getString(rt).utf8(rt);
      auto existingText = takeFirstUnkeyed(WorkTag::HostText, nullptr);
      std::shared_ptr<FiberNode> fiber;
      if (existingText) {
        jsi::String textValue = jsi::String::createFromUtf8(rt, text);
        auto pendingProps = jsi::Value(rt, textValue);
        fiber = cloneFiberForReuse(rt, existingText, std::move(pendingProps), jsi::Value::undefined());
      } else {
        fiber = createFiberFromTextPlaceholder(rt, text);
      }
      if (fiber) {
        placeChild(result, fiber, index, true);
        ++index;
      }
      continue;
    }

    if (value.isNumber()) {
      std::string text = numberToText(value.getNumber());
      auto existingText = takeFirstUnkeyed(WorkTag::HostText, nullptr);
      std::shared_ptr<FiberNode> fiber;
      if (existingText) {
        jsi::String textValue = jsi::String::createFromUtf8(rt, text);
        auto pendingProps = jsi::Value(rt, textValue);
        fiber = cloneFiberForReuse(rt, existingText, std::move(pendingProps), jsi::Value::undefined());
      } else {
        fiber = createFiberFromTextPlaceholder(rt, text);
      }
      if (fiber) {
        placeChild(result, fiber, index, true);
        ++index;
      }
      continue;
    }

    if (!value.isObject()) {
      continue;
    }

    jsi::Object obj = value.asObject(rt);
    if (obj.isArray(rt)) {
      jsi::Array nested = obj.asArray(rt);
      size_t nestedLength = nested.size(rt);
      for (size_t j = 0; j < nestedLength; ++j) {
        jsi::Value nestedValue = nested.getValueAtIndex(rt, j);
        if (nestedValue.isUndefined() || nestedValue.isNull()) {
          continue;
        }
        if (nestedValue.isBool() && !nestedValue.getBool()) {
          continue;
        }

        std::shared_ptr<FiberNode> childFiber;
        if (nestedValue.isString()) {
          childFiber = createFiberFromTextPlaceholder(rt, nestedValue.getString(rt).utf8(rt));
        } else if (nestedValue.isNumber()) {
          childFiber = createFiberFromTextPlaceholder(rt, numberToText(nestedValue.getNumber()));
        } else if (nestedValue.isObject()) {
          childFiber = createFiberFromElementPlaceholder(rt, nestedValue.asObject(rt));
        }

        if (childFiber) {
          placeChild(result, childFiber, index, true);
          ++index;
        }
      }
      continue;
    }

    jsi::Value typeValue = obj.getProperty(rt, "type");
    jsi::Value propsValue = obj.hasProperty(rt, "props")
      ? obj.getProperty(rt, "props")
      : jsi::Value::undefined();
    jsi::Value keyValue = obj.hasProperty(rt, "key")
      ? obj.getProperty(rt, "key")
      : jsi::Value::undefined();

    auto keyOpt = extractKeyString(rt, keyValue);
    std::shared_ptr<FiberNode> childFiber;

    if (keyOpt) {
      auto it = keyedExisting.find(*keyOpt);
      if (it != keyedExisting.end()) {
        auto matchedExisting = it->second;
        if (
          matchedExisting &&
          matchedExisting->tag == WorkTag::HostComponent &&
          typeValue.isString() &&
          matchedExisting->type.isString() &&
          matchedExisting->type.asString(rt).utf8(rt) == typeValue.asString(rt).utf8(rt)
        ) {
          auto pendingProps = propsValue.isUndefined()
            ? jsi::Value::undefined()
            : jsi::Value(rt, propsValue);
          jsi::Value keyCopy = keyValue.isUndefined() || keyValue.isNull()
            ? jsi::Value::undefined()
            : jsi::Value(rt, keyValue);

          childFiber = cloneFiberForReuse(rt, matchedExisting, std::move(pendingProps), std::move(keyCopy));
          reused.insert(matchedExisting.get());
          keyedExisting.erase(it);
        }
      }
    }

    if (!childFiber && typeValue.isString()) {
      auto reuseCandidate = takeFirstUnkeyed(WorkTag::HostComponent, [&](const std::shared_ptr<FiberNode>& candidate) {
        if (!candidate->type.isString()) {
          return false;
        }
        return candidate->type.asString(rt).utf8(rt) == typeValue.asString(rt).utf8(rt);
      });

      if (reuseCandidate) {
        auto pendingProps = propsValue.isUndefined()
          ? jsi::Value::undefined()
          : jsi::Value(rt, propsValue);
        childFiber = cloneFiberForReuse(rt, reuseCandidate, std::move(pendingProps), jsi::Value::undefined());
      }
    }

    if (!childFiber) {
      childFiber = createFiberFromElementPlaceholder(rt, obj);
    }

    if (childFiber) {
      placeChild(result, childFiber, index, true);
      ++index;
    }
  }

  for (auto& entry : existingEntries) {
    if (!entry.fiber) {
      continue;
    }
    if (reused.find(entry.fiber.get()) != reused.end()) {
      continue;
    }
    markChildForDeletion(result, entry.fiber);
  }

  return result;
}

} // namespace react
