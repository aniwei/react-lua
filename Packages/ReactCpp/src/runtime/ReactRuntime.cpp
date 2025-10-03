#include "ReactRuntime.h"

#include "jsi/jsi.h"
#include "react-dom/client/ReactDOMComponent.h"
#include "runtime/ReactHostInterface.h"
#include "runtime/ReactWasmBridge.h"
#include "runtime/ReactWasmLayout.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using facebook::jsi::Array;
using facebook::jsi::Object;
using facebook::jsi::Runtime;
using facebook::jsi::Value;

std::string numberToString(double value) {
  if (std::isnan(value) || !std::isfinite(value)) {
    return std::string{};
  }

  if (std::floor(value) == value) {
    std::ostringstream oss;
    oss << static_cast<long long>(value);
    return oss.str();
  }

  std::ostringstream oss;
  oss << value;
  return oss.str();
}

std::string valueToString(Runtime& rt, const Value& value) {
  if (value.isString()) {
    return value.getString(rt).utf8(rt);
  }

  if (value.isNumber()) {
    return numberToString(value.getNumber());
  }

  return std::string{};
}

void collectChildValues(Runtime& rt, const Value& value, std::vector<Value>& out) {
  if (value.isUndefined() || value.isNull() || value.isBool()) {
    return;
  }

  if (value.isString() || value.isNumber()) {
    out.emplace_back(rt, value);
    return;
  }

  if (!value.isObject()) {
    return;
  }

  Object obj = value.getObject(rt);
  if (obj.isArray(rt)) {
    Array array = obj.asArray(rt);
    const size_t length = array.size(rt);
    for (size_t i = 0; i < length; ++i) {
      Value entry = array.getValueAtIndex(rt, i);
      collectChildValues(rt, entry, out);
    }
    return;
  }

  if (obj.hasProperty(rt, "type")) {
    out.emplace_back(rt, value);
  }
}

struct ElementExtraction {
  std::string type;
  std::string key;
  Object propsObject;
  std::unordered_map<std::string, Value> propsMap;
  Value children;
};

ElementExtraction extractElement(Runtime& rt, const Object& element) {
  ElementExtraction extraction{std::string{}, std::string{}, Object(rt), std::unordered_map<std::string, Value>{}, Value::undefined()};

  Value typeValue = element.getProperty(rt, "type");
  if (!typeValue.isString()) {
    return extraction;
  }
  extraction.type = typeValue.getString(rt).utf8(rt);

  Value keyValue = element.getProperty(rt, "key");
  if (keyValue.isString()) {
    extraction.key = keyValue.getString(rt).utf8(rt);
  }

  Value propsValue = element.getProperty(rt, "props");
  if (propsValue.isObject()) {
    Object propsObject = propsValue.getObject(rt);
    Array names = propsObject.getPropertyNames(rt);
    const size_t length = names.size(rt);
    for (size_t i = 0; i < length; ++i) {
      Value nameValue = names.getValueAtIndex(rt, i);
      if (!nameValue.isString()) {
        continue;
      }

      const std::string propName = nameValue.getString(rt).utf8(rt);
      Value propEntry = propsObject.getProperty(rt, propName.c_str());
      if (propName == "children") {
        extraction.children = Value(rt, propEntry);
        continue;
      }

      extraction.propsObject.setProperty(rt, propName.c_str(), propEntry);
      extraction.propsMap.emplace(propName, Value(rt, propEntry));
    }
  }

  return extraction;
}

Object createPropsObject(Runtime& rt, const std::unordered_map<std::string, Value>& propsMap) {
  Object props(rt);
  for (const auto& entry : propsMap) {
    props.setProperty(rt, entry.first.c_str(), Value(rt, entry.second));
  }
  return props;
}

bool valuesEqual(Runtime& rt, const Value& a, const Value& b) {
  if (a.isUndefined() && b.isUndefined()) {
    return true;
  }
  if (a.isNull() && b.isNull()) {
    return true;
  }
  if (a.isBool() && b.isBool()) {
    return a.getBool() == b.getBool();
  }
  if (a.isNumber() && b.isNumber()) {
    return a.getNumber() == b.getNumber();
  }
  if (a.isString() && b.isString()) {
    return a.getString(rt).utf8(rt) == b.getString(rt).utf8(rt);
  }
  return false;
}

bool computeUpdatePayload(
    Runtime& rt,
    const std::unordered_map<std::string, Value>& prevProps,
    const std::unordered_map<std::string, Value>& nextProps,
    Object& payload) {
  bool hasChanges = false;

  Object attributes(rt);
  bool hasAttributeChanges = false;

  for (const auto& [key, nextValue] : nextProps) {
    auto it = prevProps.find(key);
    if (it == prevProps.end() || !valuesEqual(rt, it->second, nextValue)) {
      attributes.setProperty(rt, key.c_str(), Value(rt, nextValue));
      hasAttributeChanges = true;
      hasChanges = true;
    }
  }

  if (hasAttributeChanges) {
    payload.setProperty(rt, "attributes", attributes);
  }

  std::vector<std::string> removedKeys;
  removedKeys.reserve(prevProps.size());
  for (const auto& [key, prevValue] : prevProps) {
    (void)prevValue;
    if (nextProps.find(key) == nextProps.end()) {
      removedKeys.push_back(key);
    }
  }

  if (!removedKeys.empty()) {
    Array removedArray(rt, removedKeys.size());
    for (size_t i = 0; i < removedKeys.size(); ++i) {
      removedArray.setValueAtIndex(rt, i, facebook::jsi::String::createFromUtf8(rt, removedKeys[i]));
    }
    payload.setProperty(rt, "removedAttributes", removedArray);
    hasChanges = true;
  }

  return hasChanges;
}

void reconcileChildren(
    react::ReactRuntime& runtime,
    Runtime& rt,
    const std::shared_ptr<react::ReactDOMInstance>& parent,
    const Value& childrenValue);

std::shared_ptr<react::ReactDOMInstance> mountElement(
    react::ReactRuntime& runtime,
    Runtime& rt,
    const ElementExtraction& extraction,
    const std::shared_ptr<react::ReactDOMInstance>& existing) {
  if (extraction.type.empty()) {
    return nullptr;
  }

  std::shared_ptr<react::ReactDOMInstance> instance = existing;
  auto existingComponent = std::dynamic_pointer_cast<react::ReactDOMComponent>(existing);

  if (!instance || !existingComponent || existingComponent->getType() != extraction.type) {
    instance = runtime.createInstance(rt, extraction.type, extraction.propsObject);
    instance->setKey(extraction.key);
    reconcileChildren(runtime, rt, instance, extraction.children);
    return instance;
  }

  instance->setKey(extraction.key);

  const auto& previousProps = existingComponent->getProps();
  Object payload(rt);
  if (computeUpdatePayload(rt, previousProps, extraction.propsMap, payload)) {
    Object oldProps = createPropsObject(rt, previousProps);
    runtime.commitUpdate(instance, oldProps, extraction.propsObject, payload);
  }

  reconcileChildren(runtime, rt, instance, extraction.children);
  return instance;
}

void removeAllChildren(
    react::ReactRuntime& runtime,
    const std::shared_ptr<react::ReactDOMInstance>& parent) {
  if (!parent) {
    return;
  }

  auto component = std::dynamic_pointer_cast<react::ReactDOMComponent>(parent);
  if (!component) {
    return;
  }

  auto existingChildren = component->children;
  for (const auto& child : existingChildren) {
    runtime.removeChild(parent, child);
  }
}

void reconcileChildren(
    react::ReactRuntime& runtime,
    Runtime& rt,
    const std::shared_ptr<react::ReactDOMInstance>& parent,
    const Value& childrenValue) {
  auto parentComponent = std::dynamic_pointer_cast<react::ReactDOMComponent>(parent);
  if (!parentComponent || parentComponent->isTextInstance()) {
    return;
  }

  std::vector<Value> desiredValues;
  collectChildValues(rt, childrenValue, desiredValues);

  std::unordered_map<std::string, std::shared_ptr<react::ReactDOMInstance>> keyedExisting;
  std::vector<std::shared_ptr<react::ReactDOMInstance>> unkeyedElements;
  std::vector<std::shared_ptr<react::ReactDOMInstance>> unkeyedText;

  for (const auto& child : parentComponent->children) {
    auto component = std::dynamic_pointer_cast<react::ReactDOMComponent>(child);
    if (!component) {
      continue;
    }

    const std::string& childKey = child->getKey();
    if (!childKey.empty()) {
      keyedExisting.emplace(childKey, child);
      continue;
    }

    if (component->isTextInstance()) {
      unkeyedText.push_back(child);
    } else {
      unkeyedElements.push_back(child);
    }
  }

  struct DesiredChild {
    std::shared_ptr<react::ReactDOMInstance> instance;
  };

  std::vector<DesiredChild> desiredChildren;
  desiredChildren.reserve(desiredValues.size());

  for (const auto& childValue : desiredValues) {
    if (childValue.isString() || childValue.isNumber()) {
      const std::string text = valueToString(rt, childValue);
      std::shared_ptr<react::ReactDOMInstance> existingText;
      if (!unkeyedText.empty()) {
        existingText = unkeyedText.front();
        unkeyedText.erase(unkeyedText.begin());
      }

      if (existingText) {
        auto textComponent = std::dynamic_pointer_cast<react::ReactDOMComponent>(existingText);
        if (textComponent && textComponent->getTextContent() != text) {
          runtime.commitTextUpdate(existingText, textComponent->getTextContent(), text);
        }
        desiredChildren.push_back({existingText});
      } else {
        auto textInstance = runtime.createTextInstance(rt, text);
        desiredChildren.push_back({textInstance});
      }
      continue;
    }

    if (!childValue.isObject()) {
      continue;
    }

    Object childObject = childValue.getObject(rt);
    if (!childObject.hasProperty(rt, "type")) {
      continue;
    }

    ElementExtraction extraction = extractElement(rt, childObject);
    std::shared_ptr<react::ReactDOMInstance> existingMatch;

    if (!extraction.key.empty()) {
      auto keyedIt = keyedExisting.find(extraction.key);
      if (keyedIt != keyedExisting.end()) {
        existingMatch = keyedIt->second;
        keyedExisting.erase(keyedIt);
      }
    }

    if (!existingMatch) {
      auto matchIt = std::find_if(
          unkeyedElements.begin(),
          unkeyedElements.end(),
          [&](const std::shared_ptr<react::ReactDOMInstance>& candidate) {
            auto candidateComponent = std::dynamic_pointer_cast<react::ReactDOMComponent>(candidate);
            return candidateComponent && !candidateComponent->isTextInstance() && candidateComponent->getType() == extraction.type;
          });
      if (matchIt != unkeyedElements.end()) {
        existingMatch = *matchIt;
        unkeyedElements.erase(matchIt);
      }
    }

    auto mounted = mountElement(runtime, rt, extraction, existingMatch);
    if (mounted) {
      desiredChildren.push_back({mounted});
    }
  }

  // Remove any remaining children that were not reused.
  for (auto& [_, child] : keyedExisting) {
    runtime.removeChild(parent, child);
  }
  for (auto& child : unkeyedElements) {
    runtime.removeChild(parent, child);
  }
  for (auto& child : unkeyedText) {
    runtime.removeChild(parent, child);
  }

  for (size_t index = 0; index < desiredChildren.size(); ++index) {
    auto& entry = desiredChildren[index];
    auto child = entry.instance;
    if (!child) {
      continue;
    }

    auto currentChildren = parentComponent->children;
    std::shared_ptr<react::ReactDOMInstance> beforeChild;
    if (index < currentChildren.size()) {
      beforeChild = currentChildren[index];
    }

    auto attachedParent = child->parent.lock();
    const bool isAttachedToParent = attachedParent && attachedParent.get() == parent.get();

    if (!isAttachedToParent) {
      if (beforeChild) {
        runtime.insertBefore(parent, child, beforeChild);
      } else {
        runtime.appendChild(parent, child);
      }
      continue;
    }

    currentChildren = parentComponent->children;
    if (index < currentChildren.size() && currentChildren[index].get() == child.get()) {
      continue;
    }

    runtime.insertBefore(parent, child, beforeChild);
  }
}

} // namespace

namespace react {

ReactRuntime::ReactRuntime() = default;

WorkLoopState& ReactRuntime::workLoopState() {
  return workLoopState_;
}

const WorkLoopState& ReactRuntime::workLoopState() const {
  return workLoopState_;
}

RootSchedulerState& ReactRuntime::rootSchedulerState() {
  return rootSchedulerState_;
}

const RootSchedulerState& ReactRuntime::rootSchedulerState() const {
  return rootSchedulerState_;
}

AsyncActionState& ReactRuntime::asyncActionState() {
  return asyncActionState_;
}

const AsyncActionState& ReactRuntime::asyncActionState() const {
  return asyncActionState_;
}

void ReactRuntime::resetWorkLoop() {
  workLoopState_ = WorkLoopState{};
}

void ReactRuntime::resetRootScheduler() {
  rootSchedulerState_ = RootSchedulerState{};
}

void ReactRuntime::setHostInterface(std::shared_ptr<HostInterface> hostInterface) {
  hostInterface_ = std::move(hostInterface);
}

void ReactRuntime::bindHostInterface(facebook::jsi::Runtime& runtime) {
  (void)runtime;
  // TODO: hook host interface bindings once available.
}

void ReactRuntime::reset() {
  resetWorkLoop();
  resetRootScheduler();
  asyncActionState_ = AsyncActionState{};
  registeredRoots_.clear();
}

void ReactRuntime::setShouldAttemptEagerTransitionCallback(std::function<bool()> callback) {
  shouldAttemptEagerTransitionCallback_ = std::move(callback);
}

bool ReactRuntime::shouldAttemptEagerTransition() const {
  if (shouldAttemptEagerTransitionCallback_) {
    return shouldAttemptEagerTransitionCallback_();
  }
  return false;
}

void ReactRuntime::renderRootSync(
  facebook::jsi::Runtime& runtime,
  std::uint32_t rootElementOffset,
  std::shared_ptr<ReactDOMInstance> rootContainer) {
  if (!rootContainer) {
    return;
  }

  registerRootContainer(rootContainer);
  if (rootElementOffset == 0 || __wasm_memory_buffer == nullptr) {
    removeAllChildren(*this, rootContainer);
    return;
  }

  WasmReactValue rootValue{};
  rootValue.type = WasmValueType::Element;
  rootValue.data.ptrValue = rootElementOffset;

  Value rootElement = convertWasmLayoutToJsi(runtime, 0, rootValue);
  reconcileChildren(*this, runtime, rootContainer, rootElement);
}

void ReactRuntime::hydrateRoot(
  facebook::jsi::Runtime& runtime,
  std::uint32_t rootElementOffset,
  std::shared_ptr<ReactDOMInstance> rootContainer) {
  renderRootSync(runtime, rootElementOffset, std::move(rootContainer));
}

TaskHandle ReactRuntime::scheduleTask(
  SchedulerPriority priority,
  Task task,
  const TaskOptions& options) {
  (void)options;
  const auto previous = currentPriority_;
  currentPriority_ = priority;
  if (task) {
    task();
  }
  currentPriority_ = previous;
  return TaskHandle{nextTaskId_++};
}

void ReactRuntime::cancelTask(TaskHandle handle) {
  (void)handle;
  // TODO: integrate host scheduler cancellation semantics.
}

SchedulerPriority ReactRuntime::getCurrentPriorityLevel() const {
  return currentPriority_;
}

SchedulerPriority ReactRuntime::runWithPriority(
  SchedulerPriority priority,
  const std::function<void()>& fn) {
  const auto previous = currentPriority_;
  currentPriority_ = priority;
  if (fn) {
    fn();
  }
  currentPriority_ = previous;
  return previous;
}

bool ReactRuntime::shouldYield() const {
  return false;
}

double ReactRuntime::now() const {
  const auto steadyNow = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(steadyNow).count();
}

std::shared_ptr<HostInterface> ReactRuntime::ensureHostInterface() {
  if (!hostInterface_) {
    hostInterface_ = std::make_shared<HostInterface>();
  }
  return hostInterface_;
}

std::shared_ptr<ReactDOMInstance> ReactRuntime::createInstance(
    facebook::jsi::Runtime& runtime,
    const std::string& type,
    const facebook::jsi::Object& props) {
  return ensureHostInterface()->createHostInstance(runtime, type, props);
}

std::shared_ptr<ReactDOMInstance> ReactRuntime::createTextInstance(
    facebook::jsi::Runtime& runtime,
    const std::string& text) {
  return ensureHostInterface()->createHostTextInstance(runtime, text);
}

void ReactRuntime::appendChild(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child) {
  ensureHostInterface()->appendHostChild(std::move(parent), std::move(child));
}

void ReactRuntime::removeChild(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child) {
  ensureHostInterface()->removeHostChild(std::move(parent), std::move(child));
}

void ReactRuntime::insertBefore(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child,
    std::shared_ptr<ReactDOMInstance> beforeChild) {
  ensureHostInterface()->insertHostChildBefore(std::move(parent), std::move(child), std::move(beforeChild));
}

void ReactRuntime::commitUpdate(
    std::shared_ptr<ReactDOMInstance> instance,
    const facebook::jsi::Object& oldProps,
    const facebook::jsi::Object& newProps,
    const facebook::jsi::Object& payload) {
  ensureHostInterface()->commitHostUpdate(std::move(instance), oldProps, newProps, payload);
}

void ReactRuntime::commitTextUpdate(
    std::shared_ptr<ReactDOMInstance> instance,
    const std::string& oldText,
    const std::string& newText) {
  ensureHostInterface()->commitHostTextUpdate(std::move(instance), oldText, newText);
}

void ReactRuntime::unregisterRootContainer(const ReactDOMInstance* rootContainer) {
  if (rootContainer == nullptr) {
    return;
  }
  registeredRoots_.erase(rootContainer);
}

std::size_t ReactRuntime::getRegisteredRootCount() const {
  std::size_t count = 0;
  for (const auto& entry : registeredRoots_) {
    if (!entry.second.expired()) {
      ++count;
    }
  }
  return count;
}

void ReactRuntime::registerRootContainer(const std::shared_ptr<ReactDOMInstance>& rootContainer) {
  if (!rootContainer) {
    return;
  }
  registeredRoots_[rootContainer.get()] = rootContainer;
}

} // namespace react

namespace react::ReactRuntimeTestHelper {

std::size_t getRegisteredRootCount(const ReactRuntime& runtime) {
  return runtime.getRegisteredRootCount();
}

} // namespace react::ReactRuntimeTestHelper
