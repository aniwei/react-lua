#include "runtime/ReactRuntime.h"
#include "runtime/ReactWasmBridge.h"
#include "runtime/ReactWasmLayout.h"
#include "react-dom/client/ReactDOMComponent.h"
#include "reconciler/FiberNode.h"
#include "TestRuntime.h"

#include "jsi/jsi.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

using namespace facebook::jsi;

namespace react {

namespace {
using test::TestRuntime;

struct WasmMemoryBuilder {
  WasmMemoryBuilder() {
    buffer.push_back(0); // Reserve offset 0 as null sentinel.
  }

  uint32_t appendString(const std::string& value) {
    uint32_t offset = static_cast<uint32_t>(buffer.size());
    buffer.insert(buffer.end(), value.begin(), value.end());
    buffer.push_back('\0');
    return offset;
  }

  template <typename T>
  uint32_t appendStruct(const T& value) {
    uint32_t offset = static_cast<uint32_t>(buffer.size());
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
    return offset;
  }

  uint8_t* data() {
    return buffer.data();
  }

  std::vector<uint8_t> buffer;
};

jsi::Object makeElementObject(
  jsi::Runtime& rt,
  const std::string& type,
  const std::string& key,
  const std::unordered_map<std::string, std::string>& propsMap) {
  jsi::Object element(rt);
  element.setProperty(rt, "type", jsi::String::createFromUtf8(rt, type));

  if (!key.empty()) {
    element.setProperty(rt, "key", jsi::String::createFromUtf8(rt, key));
  }

  jsi::Object props(rt);
  for (const auto& entry : propsMap) {
    props.setProperty(rt, entry.first.c_str(), jsi::String::createFromUtf8(rt, entry.second));
  }
  element.setProperty(rt, "props", props);

  return element;
}

WasmReactProp makeStringProp(uint32_t keyOffset, uint32_t valueOffset) {
  WasmReactProp prop{};
  prop.key_ptr = keyOffset;
  prop.value.type = WasmValueType::String;
  prop.value.data.ptrValue = valueOffset;
  return prop;
}

uint32_t appendProps(
    WasmMemoryBuilder& builder,
    const std::vector<WasmReactProp>& props) {
  if (props.empty()) {
    return 0;
  }
  uint32_t firstOffset = 0;
  for (size_t i = 0; i < props.size(); ++i) {
    uint32_t offset = builder.appendStruct(props[i]);
    if (i == 0) {
      firstOffset = offset;
    }
  }
  return firstOffset;
}

WasmReactValue makeElementChild(uint32_t elementOffset) {
  WasmReactValue value{};
  value.type = WasmValueType::Element;
  value.data.ptrValue = elementOffset;
  return value;
}

uint32_t appendChildren(
    WasmMemoryBuilder& builder,
    const std::vector<WasmReactValue>& children) {
  if (children.empty()) {
    return 0;
  }
  uint32_t firstOffset = 0;
  for (size_t i = 0; i < children.size(); ++i) {
    uint32_t offset = builder.appendStruct(children[i]);
    if (i == 0) {
      firstOffset = offset;
    }
  }
  return firstOffset;
}

struct WasmLayout {
  WasmMemoryBuilder builder;
  uint32_t rootOffset{0};
};

WasmLayout buildKeyedSpanLayout(const std::vector<std::pair<std::string, std::string>>& childrenSpecs) {
  WasmLayout layout;
  auto& builder = layout.builder;

  const uint32_t typeDivOffset = builder.appendString("div");
  const uint32_t typeSpanOffset = builder.appendString("span");
  const uint32_t classNameKeyOffset = builder.appendString("className");

  std::vector<WasmReactValue> childValues;
  childValues.reserve(childrenSpecs.size());

  for (const auto& [key, className] : childrenSpecs) {
    const uint32_t keyOffset = builder.appendString(key);
    const uint32_t classOffset = builder.appendString(className);

    std::vector<WasmReactProp> props;
    props.push_back(makeStringProp(classNameKeyOffset, classOffset));
    const uint32_t propsOffset = appendProps(builder, props);

    WasmReactElement element{};
    element.type_name_ptr = typeSpanOffset;
    element.key_ptr = keyOffset;
    element.ref_ptr = 0;
    element.props_count = static_cast<uint32_t>(props.size());
    element.props_ptr = propsOffset;
    element.children_count = 0;
    element.children_ptr = 0;
    const uint32_t elementOffset = builder.appendStruct(element);

    childValues.push_back(makeElementChild(elementOffset));
  }

  const uint32_t childrenOffset = appendChildren(builder, childValues);

  WasmReactElement root{};
  root.type_name_ptr = typeDivOffset;
  root.key_ptr = 0;
  root.ref_ptr = 0;
  root.props_count = 0;
  root.props_ptr = 0;
  root.children_count = static_cast<uint32_t>(childValues.size());
  root.children_ptr = childrenOffset;
  layout.rootOffset = builder.appendStruct(root);

  return layout;
}

std::shared_ptr<ReactDOMInstance> findChildByKey(
    const std::shared_ptr<ReactDOMComponent>& parent,
    const std::string& key) {
  if (!parent) {
    return nullptr;
  }
  for (const auto& child : parent->children) {
    if (child && child->getKey() == key) {
      return child;
    }
  }
  return nullptr;
}

bool hasFlag(FiberFlags value, FiberFlags flag) {
  return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

// Helper for test assertions.
struct TestContext {
  TestRuntime runtime;
  ReactRuntime reactRuntime;
};

struct RecordingHostInterface : HostInterface {
  std::vector<std::string> log;

  std::shared_ptr<ReactDOMInstance> createHostInstance(
      facebook::jsi::Runtime& runtime,
      const std::string& type,
      const facebook::jsi::Object& props) override {
    log.emplace_back("create:" + type);
    return HostInterface::createHostInstance(runtime, type, props);
  }

  std::shared_ptr<ReactDOMInstance> createHostTextInstance(
      facebook::jsi::Runtime& runtime,
      const std::string& text) override {
    log.emplace_back("createText:" + text);
    return HostInterface::createHostTextInstance(runtime, text);
  }

  void appendHostChild(
      std::shared_ptr<ReactDOMInstance> parent,
      std::shared_ptr<ReactDOMInstance> child) override {
    log.emplace_back("append:" + describeInstance(child));
    HostInterface::appendHostChild(parent, child);
  }

  void removeHostChild(
      std::shared_ptr<ReactDOMInstance> parent,
      std::shared_ptr<ReactDOMInstance> child) override {
    log.emplace_back("remove:" + describeInstance(child));
    HostInterface::removeHostChild(parent, child);
  }

  void insertHostChildBefore(
      std::shared_ptr<ReactDOMInstance> parent,
      std::shared_ptr<ReactDOMInstance> child,
      std::shared_ptr<ReactDOMInstance> beforeChild) override {
    log.emplace_back("insertBefore:" + describeInstance(child) + "->" + describeInstance(beforeChild));
    HostInterface::insertHostChildBefore(parent, child, beforeChild);
  }

  void commitHostUpdate(
      std::shared_ptr<ReactDOMInstance> instance,
      const facebook::jsi::Object& oldProps,
      const facebook::jsi::Object& newProps,
      const facebook::jsi::Object& payload) override {
    log.emplace_back("commit:" + describeInstance(instance));
    HostInterface::commitHostUpdate(instance, oldProps, newProps, payload);
  }

  void commitHostTextUpdate(
      std::shared_ptr<ReactDOMInstance> instance,
      const std::string& oldText,
      const std::string& newText) override {
    log.emplace_back("commitText:" + describeInstance(instance) + ":" + oldText + "->" + newText);
    HostInterface::commitHostTextUpdate(instance, oldText, newText);
  }

private:
  static std::string describeInstance(const std::shared_ptr<ReactDOMInstance>& instance) {
    if (!instance) {
      return std::string("<null>");
    }
    auto component = std::dynamic_pointer_cast<ReactDOMComponent>(instance);
    if (!component) {
      return std::string("<unknown>");
    }
    std::string description = component->isTextInstance() ? std::string("#text") : component->getType();
    const std::string& key = instance->getKey();
    if (!key.empty()) {
      description.append("(").append(key).append(")");
    }
    return description;
  }
};

bool testHostComponentPayload(TestContext& ctx) {
  auto& rt = ctx.runtime;

  Object prevProps(rt);
  prevProps.setProperty(rt, "id", String::createFromUtf8(rt, "foo"));
  prevProps.setProperty(rt, "className", String::createFromUtf8(rt, "old"));
  prevProps.setProperty(rt, "data-old", String::createFromUtf8(rt, "1"));

  Object nextProps(rt);
  nextProps.setProperty(rt, "id", String::createFromUtf8(rt, "foo"));
  nextProps.setProperty(rt, "className", String::createFromUtf8(rt, "new"));
  nextProps.setProperty(rt, "title", String::createFromUtf8(rt, "hello"));

  Value payload;
  bool hasUpdate = ReactRuntimeTestHelper::computeHostComponentUpdatePayload(
    rt,
    Value(rt, prevProps),
    Value(rt, nextProps),
    payload);
  if (!hasUpdate) {
    std::cerr << "Expected payload to be generated for host component" << std::endl;
    return false;
  }

  auto payloadObj = payload.asObject(rt);

  auto attributesValue = payloadObj.getProperty(rt, "attributes");
  if (!attributesValue.isObject()) {
    std::cerr << "Expected payload.attributes to be object" << std::endl;
    return false;
  }

  auto attributesObj = attributesValue.asObject(rt);
  auto classNameValue = attributesObj.getProperty(rt, "className");
  if (!classNameValue.isString() || classNameValue.asString(rt).utf8(rt) != "new") {
    std::cerr << "Expected className to be updated" << std::endl;
    return false;
  }

  auto titleValue = attributesObj.getProperty(rt, "title");
  if (!titleValue.isString() || titleValue.asString(rt).utf8(rt) != "hello") {
    std::cerr << "Expected title to be added" << std::endl;
    return false;
  }

  auto removedValue = payloadObj.getProperty(rt, "removedAttributes");
  if (!removedValue.isObject() || !removedValue.asObject(rt).isArray(rt)) {
    std::cerr << "Expected removedAttributes to be array" << std::endl;
    return false;
  }

  auto removedArray = removedValue.asObject(rt).asArray(rt);
  if (removedArray.size(rt) != 1) {
    std::cerr << "Expected one removed attribute" << std::endl;
    return false;
  }

  auto removedEntry = removedArray.getValueAtIndex(rt, 0);
  if (!removedEntry.isString() || removedEntry.asString(rt).utf8(rt) != "data-old") {
    std::cerr << "Expected removed attribute to be data-old" << std::endl;
    return false;
  }

  return true;
}

bool testHostTextPayload(TestContext& ctx) {
  auto& rt = ctx.runtime;

  Value payload;
  bool hasUpdate = ReactRuntimeTestHelper::computeHostTextUpdatePayload(
    rt,
    Value(String::createFromUtf8(rt, "old")),
    Value(String::createFromUtf8(rt, "new")),
    payload);
  if (!hasUpdate) {
    std::cerr << "Expected text payload to be generated" << std::endl;
    return false;
  }

  auto payloadObj = payload.asObject(rt);
  auto textValue = payloadObj.getProperty(rt, "text");
  if (!textValue.isString() || textValue.asString(rt).utf8(rt) != "new") {
    std::cerr << "Expected text payload to contain new text" << std::endl;
    return false;
  }

  return true;
}

bool testCloneFiberSetsUpdate(TestContext& ctx) {
  auto& rt = ctx.runtime;
  auto& react = ctx.reactRuntime;

  Object prevProps(rt);
  prevProps.setProperty(rt, "className", String::createFromUtf8(rt, "old"));

  Object nextProps(rt);
  nextProps.setProperty(rt, "className", String::createFromUtf8(rt, "new"));

  auto existing = std::make_shared<FiberNode>(
    WorkTag::HostComponent,
    Value(rt, prevProps),
    Value::undefined());
  existing->memoizedProps = Value(rt, prevProps);

  auto cloned = ReactRuntimeTestHelper::cloneFiberForReuse(
    react,
    rt,
    existing,
    Value(rt, nextProps),
    Value::undefined());

  if (!cloned) {
    std::cerr << "Expected cloneFiberForReuse to return a fiber" << std::endl;
    return false;
  }

  bool hasUpdateFlag = (static_cast<uint32_t>(cloned->flags) & static_cast<uint32_t>(FiberFlags::Update)) != 0;
  if (!hasUpdateFlag) {
    std::cerr << "Expected cloned fiber to carry Update flag" << std::endl;
    return false;
  }

  if (cloned->updatePayload.isUndefined()) {
    std::cerr << "Expected cloned fiber to carry update payload" << std::endl;
    return false;
  }

  auto payloadObj = cloned->updatePayload.asObject(rt);
  auto attributesValue = payloadObj.getProperty(rt, "attributes");
  if (!attributesValue.isObject()) {
    std::cerr << "Expected payload.attributes to exist" << std::endl;
    return false;
  }

  auto attributeObj = attributesValue.asObject(rt);
  auto classNameValue = attributeObj.getProperty(rt, "className");
  if (!classNameValue.isString() || classNameValue.asString(rt).utf8(rt) != "new") {
    std::cerr << "Expected payload.attributes.className to equal new" << std::endl;
    return false;
  }

  return true;
}

bool testReactDOMComponentApplyUpdate(TestContext& ctx) {
  auto& rt = ctx.runtime;

  Object initialProps(rt);
  initialProps.setProperty(rt, "className", String::createFromUtf8(rt, "old"));
  initialProps.setProperty(rt, "title", String::createFromUtf8(rt, "hello"));

  ReactDOMComponent instance(rt, "div", initialProps);

  Object newProps(rt);
  newProps.setProperty(rt, "className", String::createFromUtf8(rt, "new"));

  Object payload(rt);
  Object attributes(rt);
  attributes.setProperty(rt, "className", String::createFromUtf8(rt, "new"));
  payload.setProperty(rt, "attributes", Value(rt, attributes));

  Array removed = rt.makeArray(1);
  removed.setValueAtIndex(rt, 0, String::createFromUtf8(rt, "title"));
  payload.setProperty(rt, "removedAttributes", Value(rt, removed));

  instance.applyUpdate(newProps, payload);

  const auto& props = instance.getProps();
  auto classNameIt = props.find("className");
  if (classNameIt == props.end()) {
    std::cerr << "Expected className prop after update" << std::endl;
    return false;
  }

  if (!classNameIt->second.isString() || classNameIt->second.asString(rt).utf8(rt) != "new") {
    std::cerr << "Expected className to be \"new\"" << std::endl;
    return false;
  }

  if (props.find("title") != props.end()) {
    std::cerr << "Expected title prop to be removed" << std::endl;
    return false;
  }

  return true;
}

bool testReactDOMComponentDomApi(TestContext& ctx) {
  auto& rt = ctx.runtime;

  Object parentProps(rt);
  parentProps.setProperty(rt, "className", String::createFromUtf8(rt, "root"));
  auto parent = std::make_shared<ReactDOMComponent>(rt, "div", parentProps);

  Object childOneProps(rt);
  childOneProps.setProperty(rt, "className", String::createFromUtf8(rt, "child-one"));
  auto childOne = std::make_shared<ReactDOMComponent>(rt, "span", childOneProps);

  Object childTwoProps(rt);
  childTwoProps.setProperty(rt, "className", String::createFromUtf8(rt, "child-two"));
  auto childTwo = std::make_shared<ReactDOMComponent>(rt, "span", childTwoProps);

  parent->appendChild(childOne);
  parent->insertChildBefore(childTwo, childOne);

  if (parent->children.size() != 2) {
    std::cerr << "Expected parent to contain two children" << std::endl;
    return false;
  }

  if (parent->children[0].get() != childTwo.get() || parent->children[1].get() != childOne.get()) {
    std::cerr << "Expected insertChildBefore to reorder children" << std::endl;
    return false;
  }

  if (childOne->parent.lock().get() != parent.get()) {
    std::cerr << "Expected childOne to reference parent" << std::endl;
    return false;
  }

  parent->removeChild(childTwo);
  if (parent->children.size() != 1 || parent->children[0].get() != childOne.get()) {
    std::cerr << "Expected removeChild to detach childTwo" << std::endl;
    return false;
  }
  if (!childTwo->parent.expired()) {
    std::cerr << "Expected removed child to clear parent weak pointer" << std::endl;
    return false;
  }

  // HostObject view
  Object parentHost = Object::createFromHostObject(rt, parent);
  auto classNameValue = parentHost.getProperty(rt, "className");
  if (!classNameValue.isString() || classNameValue.asString(rt).utf8(rt) != "root") {
    std::cerr << "Expected className getter to reflect initial value" << std::endl;
    return false;
  }

  parentHost.setProperty(rt, "className", String::createFromUtf8(rt, "updated"));
  if (parent->className != "updated") {
    std::cerr << "Expected className setter to update underlying instance" << std::endl;
    return false;
  }

  parentHost.setProperty(rt, "textContent", String::createFromUtf8(rt, "hello"));
  if (parent->getTextContent() != "hello") {
    std::cerr << "Expected textContent setter to update host instance" << std::endl;
    return false;
  }

  auto textValue = parentHost.getProperty(rt, "textContent");
  if (!textValue.isString() || textValue.asString(rt).utf8(rt) != "hello") {
    std::cerr << "Expected textContent getter to return updated value" << std::endl;
    return false;
  }

  auto childrenValue = parentHost.getProperty(rt, "children");
  if (!childrenValue.isObject() || !childrenValue.asObject(rt).isArray(rt)) {
    std::cerr << "Expected children getter to return array" << std::endl;
    return false;
  }

  parentHost.setProperty(rt, "onClick", Object::createFromHostObject(rt, childOne));
  auto handlerValue = parentHost.getProperty(rt, "onClick");
  if (!handlerValue.isObject()) {
    std::cerr << "Expected onClick setter to store object value" << std::endl;
    return false;
  }
  parentHost.setProperty(rt, "onClick", Value::null());
  if (!parent->getEventHandlers().empty()) {
    std::cerr << "Expected removing onClick to clear event handler map" << std::endl;
    return false;
  }

  parent->setAttribute("data-role", Value(rt, String::createFromUtf8(rt, "button")));
  auto attrValue = parent->getAttribute(rt, "data-role");
  if (!attrValue.isString() || attrValue.asString(rt).utf8(rt) != "button") {
    std::cerr << "Expected custom attribute to be retrievable" << std::endl;
    return false;
  }

  parent->removeAttribute("data-role");
  auto removedAttr = parent->getAttribute(rt, "data-role");
  if (!removedAttr.isUndefined()) {
    std::cerr << "Expected removed attribute to be undefined" << std::endl;
    return false;
  }

  return true;
}

bool testCommitAppliesHostComponentUpdate(TestContext& ctx) {
  auto& rt = ctx.runtime;
  auto& runtime = ctx.reactRuntime;

  Object rootProps(rt);
  auto rootInstance = std::make_shared<ReactDOMComponent>(rt, "root", rootProps);

  auto rootFiber = std::make_shared<FiberNode>(
    WorkTag::HostRoot,
    Value::undefined(),
    Value::undefined());
  rootFiber->stateNode = rootInstance;

  Object prevProps(rt);
  prevProps.setProperty(rt, "className", String::createFromUtf8(rt, "old"));
  prevProps.setProperty(rt, "data-old", String::createFromUtf8(rt, "1"));

  Object nextProps(rt);
  nextProps.setProperty(rt, "className", String::createFromUtf8(rt, "new"));
  nextProps.setProperty(rt, "title", String::createFromUtf8(rt, "hello"));

  auto hostInstance = std::make_shared<ReactDOMComponent>(rt, "div", prevProps);
  rootInstance->appendChild(hostInstance);

  auto currentFiber = std::make_shared<FiberNode>(
    WorkTag::HostComponent,
    Value(rt, prevProps),
    Value::undefined());
  currentFiber->memoizedProps = Value(rt, prevProps);
  currentFiber->stateNode = hostInstance;
  currentFiber->returnFiber = rootFiber;

  auto updatedFiber = std::make_shared<FiberNode>(
    WorkTag::HostComponent,
    Value(rt, nextProps),
    Value::undefined());
  updatedFiber->memoizedProps = Value(rt, nextProps);
  updatedFiber->stateNode = hostInstance;
  updatedFiber->returnFiber = rootFiber;
  updatedFiber->alternate = currentFiber;
  currentFiber->alternate = updatedFiber;

  Value payload;
  bool hasPayload = ReactRuntimeTestHelper::computeHostComponentUpdatePayload(
    rt,
    currentFiber->memoizedProps,
    updatedFiber->memoizedProps,
    payload);
  if (!hasPayload) {
    std::cerr << "Expected payload for host component update" << std::endl;
    return false;
  }

  updatedFiber->flags = FiberFlags::Update;
  updatedFiber->updatePayload = std::move(payload);
  rootFiber->child = updatedFiber;

  ReactRuntimeTestHelper::commitMutationEffects(runtime, rt, rootFiber);

  const auto& updatedProps = hostInstance->getProps();
  auto classNameIt = updatedProps.find("className");
  if (classNameIt == updatedProps.end() || !classNameIt->second.isString() || classNameIt->second.asString(rt).utf8(rt) != "new") {
    std::cerr << "Expected commit to update className" << std::endl;
    return false;
  }

  if (updatedProps.find("title") == updatedProps.end()) {
    std::cerr << "Expected commit to add new title prop" << std::endl;
    return false;
  }

  if (updatedProps.find("data-old") != updatedProps.end()) {
    std::cerr << "Expected commit to remove stale props" << std::endl;
    return false;
  }

  if (!updatedFiber->updatePayload.isUndefined()) {
    std::cerr << "Expected commit to clear update payload" << std::endl;
    return false;
  }

  bool updateFlagCleared = (static_cast<uint32_t>(updatedFiber->flags) & static_cast<uint32_t>(FiberFlags::Update)) == 0;
  if (!updateFlagCleared) {
    std::cerr << "Expected commit to clear update flag" << std::endl;
    return false;
  }

  return true;
}

bool testSimpleHostTextUpdate(TestContext& ctx) {
  auto& rt = ctx.runtime;

  ReactDOMComponent textInstance(rt, "#text", "hello");
  Object payload(rt);
  payload.setProperty(rt, "text", String::createFromUtf8(rt, "world"));
  Object newProps(rt);
  newProps.setProperty(rt, "text", String::createFromUtf8(rt, "world"));

  textInstance.applyUpdate(newProps, payload);

  if (textInstance.getTextContent() != "world") {
    std::cerr << "Expected text instance content to update" << std::endl;
    return false;
  }

  return true;
}

bool testRenderConvertsWasmLayout(TestContext& ctx) {
  ctx.reactRuntime.reset();

  WasmMemoryBuilder builder;

  const uint32_t typeDivOffset = builder.appendString("div");
  const uint32_t classNameKeyOffset = builder.appendString("className");
  const uint32_t classNameValueOffset = builder.appendString("root");
  const uint32_t textValueOffset = builder.appendString("Hello from Wasm");

  WasmReactProp classNameProp{};
  classNameProp.key_ptr = classNameKeyOffset;
  classNameProp.value.type = WasmValueType::String;
  classNameProp.value.data.ptrValue = classNameValueOffset;
  const uint32_t propsOffset = builder.appendStruct(classNameProp);

  WasmReactValue childValue{};
  childValue.type = WasmValueType::String;
  childValue.data.ptrValue = textValueOffset;
  const uint32_t childrenOffset = builder.appendStruct(childValue);

  WasmReactElement rootElement{};
  rootElement.type_name_ptr = typeDivOffset;
  rootElement.key_ptr = 0;
  rootElement.ref_ptr = 0;
  rootElement.props_count = 1;
  rootElement.props_ptr = propsOffset;
  rootElement.children_count = 1;
  rootElement.children_ptr = childrenOffset;

  const uint32_t rootElementOffset = builder.appendStruct(rootElement);

  __wasm_memory_buffer = builder.data();

  WasmReactValue rootValue{};
  rootValue.type = WasmValueType::Element;
  rootValue.data.ptrValue = rootElementOffset;

  auto& rt = ctx.runtime;

  bool ok = true;
  facebook::jsi::Value elementValue = convertWasmLayoutToJsi(rt, 0, rootValue);
  if (!elementValue.isObject()) {
    std::cerr << "Expected Wasm conversion to yield an object" << std::endl;
    ok = false;
  } else {
    auto elementObject = elementValue.asObject(rt);

    auto typeValue = elementObject.getProperty(rt, "type");
    if (!typeValue.isString() || typeValue.asString(rt).utf8(rt) != "div") {
      std::cerr << "Expected element type to be div" << std::endl;
      ok = false;
    }

    auto keyValue = elementObject.getProperty(rt, "key");
    if (!keyValue.isNull()) {
      std::cerr << "Expected key to be null when omitted" << std::endl;
      ok = false;
    }

    auto refValue = elementObject.getProperty(rt, "ref");
    if (!refValue.isNull()) {
      std::cerr << "Expected ref to be null when omitted" << std::endl;
      ok = false;
    }

    auto propsValue = elementObject.getProperty(rt, "props");
    if (!propsValue.isObject()) {
      std::cerr << "Expected element props to be an object" << std::endl;
      ok = false;
    } else {
      auto propsObject = propsValue.asObject(rt);
      auto className = propsObject.getProperty(rt, "className");
      if (!className.isString() || className.asString(rt).utf8(rt) != "root") {
        std::cerr << "Expected props.className to deserialize correctly" << std::endl;
        ok = false;
      }

      auto children = propsObject.getProperty(rt, "children");
      if (!children.isString() || children.asString(rt).utf8(rt) != "Hello from Wasm") {
        std::cerr << "Expected props.children to deserialize string child" << std::endl;
        ok = false;
      }
    }
  }

  __wasm_memory_buffer = nullptr;
  ctx.reactRuntime.reset();

  return ok;
}

bool testBridgeRenderRegistersRoot(TestContext& ctx) {
  ctx.reactRuntime.reset();
  react_reset_runtime();

  WasmMemoryBuilder builder;

  const uint32_t typeDivOffset = builder.appendString("div");
  const uint32_t propsKeyOffset = builder.appendString("id");
  const uint32_t propsValueOffset = builder.appendString("root");

  WasmReactProp idProp{};
  idProp.key_ptr = propsKeyOffset;
  idProp.value.type = WasmValueType::String;
  idProp.value.data.ptrValue = propsValueOffset;
  const uint32_t propsOffset = builder.appendStruct(idProp);

  WasmReactElement rootElement{};
  rootElement.type_name_ptr = typeDivOffset;
  rootElement.key_ptr = 0;
  rootElement.ref_ptr = 0;
  rootElement.props_count = 1;
  rootElement.props_ptr = propsOffset;
  rootElement.children_count = 0;
  rootElement.children_ptr = 0;

  const uint32_t rootElementOffset = builder.appendStruct(rootElement);

  __wasm_memory_buffer = builder.data();

  auto& rt = ctx.runtime;
  auto& runtime = ctx.reactRuntime;

  Object rootProps(rt);
  auto rootContainer = std::make_shared<ReactDOMComponent>(rt, "root", rootProps);

  react_attach_runtime(&runtime);
  react_attach_jsi_runtime(&rt);
  react_register_root_container(1234, rootContainer.get());

  size_t beforeCount = ReactRuntimeTestHelper::getRegisteredRootCount(runtime);

  react_render(rootElementOffset, 1234);

  size_t afterCount = ReactRuntimeTestHelper::getRegisteredRootCount(runtime);

  bool ok = true;
  if (afterCount == 0 || afterCount < beforeCount) {
    std::cerr << "Expected bridge render to register a root container" << std::endl;
    ok = false;
  }

  react_clear_root_container(1234);
  react_reset_runtime();
  runtime.reset();
  __wasm_memory_buffer = nullptr;

  return ok;
}

bool testRenderRootKeyedDiffReuse(TestContext& ctx) {
  ctx.reactRuntime.reset();

  auto host = std::make_shared<RecordingHostInterface>();
  ctx.reactRuntime.setHostInterface(host);
  ctx.reactRuntime.bindHostInterface(ctx.runtime);

  auto& rt = ctx.runtime;

  jsi::Object rootProps(rt);
  rootProps.setProperty(rt, "id", jsi::String::createFromUtf8(rt, "root"));
  auto rootContainer = std::make_shared<ReactDOMComponent>(rt, "div", rootProps);

  auto initialLayout = buildKeyedSpanLayout({{"a", "oldA"}, {"b", "oldB"}});
  __wasm_memory_buffer = initialLayout.builder.data();
  ctx.reactRuntime.renderRootSync(rt, initialLayout.rootOffset, rootContainer);
  __wasm_memory_buffer = nullptr;

  bool ok = true;

  auto rootComponent = std::dynamic_pointer_cast<ReactDOMComponent>(rootContainer);
  if (!rootComponent || rootComponent->children.size() != 1) {
    std::cerr << "Expected root container to have one child element" << std::endl;
    ok = false;
  }

  auto hostRoot = rootComponent && !rootComponent->children.empty()
      ? std::dynamic_pointer_cast<ReactDOMComponent>(rootComponent->children[0])
      : nullptr;

  if (!hostRoot || hostRoot->children.size() != 2) {
    std::cerr << "Expected host root to contain two child spans" << std::endl;
    ok = false;
  }

  auto childAInitial = std::dynamic_pointer_cast<ReactDOMComponent>(findChildByKey(hostRoot, "a"));
  auto childBInitial = std::dynamic_pointer_cast<ReactDOMComponent>(findChildByKey(hostRoot, "b"));

  if (!childAInitial || !childBInitial) {
    std::cerr << "Failed to locate initial keyed children" << std::endl;
    ok = false;
  }

  host->log.clear();

  auto updatedLayout = buildKeyedSpanLayout({{"a", "newA"}, {"b", "oldB"}});
  __wasm_memory_buffer = updatedLayout.builder.data();
  ctx.reactRuntime.renderRootSync(rt, updatedLayout.rootOffset, rootContainer);
  __wasm_memory_buffer = nullptr;

  hostRoot = rootComponent && !rootComponent->children.empty()
      ? std::dynamic_pointer_cast<ReactDOMComponent>(rootComponent->children[0])
      : nullptr;

  auto childAUpdated = std::dynamic_pointer_cast<ReactDOMComponent>(findChildByKey(hostRoot, "a"));
  auto childBUpdated = std::dynamic_pointer_cast<ReactDOMComponent>(findChildByKey(hostRoot, "b"));

  if (!childAUpdated || childAUpdated.get() != childAInitial.get()) {
    std::cerr << "Expected keyed child 'a' to be reused" << std::endl;
    ok = false;
  }

  if (!childBUpdated || childBUpdated.get() != childBInitial.get()) {
    std::cerr << "Expected keyed child 'b' to be reused" << std::endl;
    ok = false;
  }

  if (childAUpdated) {
    auto classValue = childAUpdated->getAttribute(rt, "className");
    if (!classValue.isString() || classValue.getString(rt).utf8(rt) != "newA") {
      std::cerr << "Expected keyed child 'a' to receive updated className" << std::endl;
      ok = false;
    }
  }

  if (host->log.size() != 1 || host->log[0] != "commit:span(a)") {
    std::cerr << "Expected exactly one commit for keyed child 'a' but host log was:" << std::endl;
    for (const auto& entry : host->log) {
      std::cerr << "  " << entry << std::endl;
    }
    ok = false;
  } else {
    const std::string& entry = host->log[0];
    if (entry.rfind("commit:", 0) != 0) {
      std::cerr << "Expected commit entry, received '" << entry << "'" << std::endl;
      ok = false;
    }
  }

  ctx.reactRuntime.setHostInterface(std::make_shared<HostInterface>());
  ctx.reactRuntime.bindHostInterface(rt);
  ctx.reactRuntime.reset();

  return ok;
}

bool testRenderRootHandlesKeyedReorderAndDeletion(TestContext& ctx) {
  ctx.reactRuntime.reset();

  auto host = std::make_shared<RecordingHostInterface>();
  ctx.reactRuntime.setHostInterface(host);
  ctx.reactRuntime.bindHostInterface(ctx.runtime);

  auto& rt = ctx.runtime;

  jsi::Object rootProps(rt);
  rootProps.setProperty(rt, "id", jsi::String::createFromUtf8(rt, "root"));
  auto rootContainer = std::make_shared<ReactDOMComponent>(rt, "div", rootProps);

  auto initialLayout = buildKeyedSpanLayout({{"a", "classA"}, {"b", "classB"}, {"c", "classC"}});
  __wasm_memory_buffer = initialLayout.builder.data();
  ctx.reactRuntime.renderRootSync(rt, initialLayout.rootOffset, rootContainer);
  __wasm_memory_buffer = nullptr;

  bool ok = true;

  auto rootComponent = std::dynamic_pointer_cast<ReactDOMComponent>(rootContainer);
  auto hostRoot = rootComponent && !rootComponent->children.empty()
      ? std::dynamic_pointer_cast<ReactDOMComponent>(rootComponent->children[0])
      : nullptr;

  if (!hostRoot || hostRoot->children.size() != 3) {
    std::cerr << "Expected host root to contain three child spans" << std::endl;
    ok = false;
  }

  auto childAInitial = findChildByKey(hostRoot, "a");
  auto childBInitial = findChildByKey(hostRoot, "b");
  auto childCInitial = findChildByKey(hostRoot, "c");

  if (!childAInitial || !childBInitial || !childCInitial) {
    std::cerr << "Failed to locate initial keyed children for reorder test" << std::endl;
    ok = false;
  }

  host->log.clear();

  auto updatedLayout = buildKeyedSpanLayout({{"c", "classC"}, {"a", "classA"}});
  __wasm_memory_buffer = updatedLayout.builder.data();
  ctx.reactRuntime.renderRootSync(rt, updatedLayout.rootOffset, rootContainer);
  __wasm_memory_buffer = nullptr;

  hostRoot = rootComponent && !rootComponent->children.empty()
      ? std::dynamic_pointer_cast<ReactDOMComponent>(rootComponent->children[0])
      : nullptr;

  if (!hostRoot || hostRoot->children.size() != 2) {
    std::cerr << "Expected host root to contain two children after deletion" << std::endl;
    ok = false;
  }

  auto childFirst = hostRoot && hostRoot->children.size() >= 1 ? hostRoot->children[0] : nullptr;
  auto childSecond = hostRoot && hostRoot->children.size() >= 2 ? hostRoot->children[1] : nullptr;

  if (!childFirst || childFirst->getKey() != "c") {
    std::cerr << "Expected first child to be keyed 'c' after reorder" << std::endl;
    ok = false;
  }

  if (!childSecond || childSecond->getKey() != "a") {
    std::cerr << "Expected second child to be keyed 'a' after reorder" << std::endl;
    ok = false;
  }

  auto childAUpdated = findChildByKey(hostRoot, "a");
  auto childCUpdated = findChildByKey(hostRoot, "c");
  auto childBUpdated = findChildByKey(hostRoot, "b");

  if (!childAUpdated || childAUpdated.get() != childAInitial.get()) {
    std::cerr << "Expected keyed child 'a' to be reused during reorder" << std::endl;
    ok = false;
  }

  if (!childCUpdated || childCUpdated.get() != childCInitial.get()) {
    std::cerr << "Expected keyed child 'c' to be reused during reorder" << std::endl;
    ok = false;
  }

  if (childBUpdated) {
    std::cerr << "Expected keyed child 'b' to be removed" << std::endl;
    ok = false;
  }

  bool sawRemoval = false;
  bool sawInsertion = false;
  for (const auto& entry : host->log) {
    if (entry == "remove:span(b)") {
      sawRemoval = true;
    }
    if (entry.rfind("insertBefore:span(c)->span(a)", 0) == 0 || entry == "insertBefore:span(c)-><null>") {
      sawInsertion = true;
    }
    if (entry.rfind("create:", 0) == 0 || entry.rfind("append:", 0) == 0) {
      std::cerr << "Did not expect new creations or appends during reorder, saw '" << entry << "'" << std::endl;
      ok = false;
    }
  }

  if (!sawRemoval) {
    std::cerr << "Expected host log to record removal of keyed child 'b'" << std::endl;
    ok = false;
  }

  if (!sawInsertion) {
    std::cerr << "Expected host log to record insertion to move keyed child 'c'" << std::endl;
    ok = false;
  }

  ctx.reactRuntime.setHostInterface(std::make_shared<HostInterface>());
  ctx.reactRuntime.bindHostInterface(rt);
  ctx.reactRuntime.reset();

  return ok;
}

bool testReconcileArrayKeyedReuseAndUpdate(TestContext& ctx) {
  ctx.reactRuntime.reset();

  auto& rt = ctx.runtime;
  auto& runtime = ctx.reactRuntime;

  auto root = std::make_shared<FiberNode>(
    WorkTag::HostRoot,
    jsi::Value::undefined(),
    jsi::Value::undefined());
  root->flags = FiberFlags::NoFlags;
  root->deletions.clear();

  auto makeHostFiber = [&](const std::string& key, const std::string& className, uint32_t index) {
    jsi::Object props(rt);
    props.setProperty(rt, "className", jsi::String::createFromUtf8(rt, className));

    jsi::String keyString = jsi::String::createFromUtf8(rt, key);
    auto fiber = std::make_shared<FiberNode>(
      WorkTag::HostComponent,
      jsi::Value::undefined(),
      jsi::Value(rt, keyString));
    fiber->memoizedProps = jsi::Value(rt, props);
    fiber->pendingProps = jsi::Value::undefined();
    fiber->type = jsi::Value(rt, jsi::String::createFromUtf8(rt, "div"));
    fiber->elementType = jsi::Value(rt, jsi::String::createFromUtf8(rt, "div"));
    fiber->index = index;
    fiber->flags = FiberFlags::NoFlags;
    return fiber;
  };

  auto childA = makeHostFiber("a", "oldA", 0);
  auto childB = makeHostFiber("b", "oldB", 1);
  childA->sibling = childB;
  childA->returnFiber = root;
  childB->returnFiber = root;
  root->child = childA;

  jsi::Array newChildren = rt.makeArray(2);
  newChildren.setValueAtIndex(rt, 0, jsi::Value(rt, makeElementObject(rt, "div", "a", { {"className", "newA"} })));
  newChildren.setValueAtIndex(rt, 1, jsi::Value(rt, makeElementObject(rt, "div", "b", { {"className", "oldB"} })));

  auto currentFirstChild = root->child;
  ReactRuntimeTestHelper::reconcileChildren(runtime, rt, root, currentFirstChild, jsi::Value(rt, newChildren));

  bool ok = true;

  auto firstChild = root->child;
  if (!firstChild) {
    std::cerr << "Expected first child after reconciliation" << std::endl;
    return false;
  }

  if (!firstChild->alternate || firstChild->alternate.get() != childA.get()) {
    std::cerr << "Expected keyed fiber 'a' to be reused" << std::endl;
    ok = false;
  }

  if (!hasFlag(firstChild->flags, FiberFlags::Update)) {
    std::cerr << "Expected reused fiber 'a' to carry Update flag" << std::endl;
    ok = false;
  }

  if (hasFlag(firstChild->flags, FiberFlags::Placement)) {
    std::cerr << "Expected reused fiber 'a' not to have Placement flag" << std::endl;
    ok = false;
  }

  if (!firstChild->updatePayload.isObject()) {
    std::cerr << "Expected update payload for fiber 'a'" << std::endl;
    ok = false;
  } else {
    auto payloadObj = firstChild->updatePayload.asObject(rt);
    auto classNameValue = payloadObj.getProperty(rt, "className");
    if (!classNameValue.isString() || classNameValue.asString(rt).utf8(rt) != "newA") {
      std::cerr << "Expected update payload to contain new className" << std::endl;
      ok = false;
    }
  }

  auto secondChild = firstChild->sibling;
  if (!secondChild) {
    std::cerr << "Expected second child after reconciliation" << std::endl;
    return false;
  }

  if (!secondChild->alternate || secondChild->alternate.get() != childB.get()) {
    std::cerr << "Expected keyed fiber 'b' to be reused" << std::endl;
    ok = false;
  }

  if (!secondChild->updatePayload.isUndefined()) {
    std::cerr << "Expected fiber 'b' to have no update payload" << std::endl;
    ok = false;
  }

  if (hasFlag(secondChild->flags, FiberFlags::Update)) {
    std::cerr << "Did not expect Update flag on unchanged fiber 'b'" << std::endl;
    ok = false;
  }

  if (hasFlag(secondChild->flags, FiberFlags::Placement)) {
    std::cerr << "Did not expect Placement flag on reused fiber 'b'" << std::endl;
    ok = false;
  }

  if (!root->deletions.empty()) {
    std::cerr << "Did not expect deletions when array lengths match" << std::endl;
    ok = false;
  }

  if (hasFlag(root->flags, FiberFlags::ChildDeletion)) {
    std::cerr << "Did not expect ChildDeletion flag on root" << std::endl;
    ok = false;
  }

  return ok;
}

bool testReconcileArrayHandlesDeletionAndPlacement(TestContext& ctx) {
  ctx.reactRuntime.reset();

  auto& rt = ctx.runtime;
  auto& runtime = ctx.reactRuntime;

  auto root = std::make_shared<FiberNode>(
    WorkTag::HostRoot,
    jsi::Value::undefined(),
    jsi::Value::undefined());
  root->flags = FiberFlags::NoFlags;
  root->deletions.clear();

  auto makeHostFiber = [&](const std::string& key, const std::string& className, uint32_t index) {
    jsi::Object props(rt);
    props.setProperty(rt, "className", jsi::String::createFromUtf8(rt, className));

    jsi::String keyString = jsi::String::createFromUtf8(rt, key);
    auto fiber = std::make_shared<FiberNode>(
      WorkTag::HostComponent,
      jsi::Value::undefined(),
      jsi::Value(rt, keyString));
    fiber->memoizedProps = jsi::Value(rt, props);
    fiber->pendingProps = jsi::Value::undefined();
    fiber->type = jsi::Value(rt, jsi::String::createFromUtf8(rt, "div"));
    fiber->elementType = jsi::Value(rt, jsi::String::createFromUtf8(rt, "div"));
    fiber->index = index;
    fiber->flags = FiberFlags::NoFlags;
    return fiber;
  };

  auto childA = makeHostFiber("a", "oldA", 0);
  auto childB = makeHostFiber("b", "oldB", 1);
  childA->sibling = childB;
  childA->returnFiber = root;
  childB->returnFiber = root;
  root->child = childA;

  jsi::Array newChildren = rt.makeArray(2);
  newChildren.setValueAtIndex(rt, 0, jsi::Value(rt, makeElementObject(rt, "div", "b", { {"className", "oldB"} })));
  newChildren.setValueAtIndex(rt, 1, jsi::Value(rt, makeElementObject(rt, "div", "c", { {"className", "newC"} })));

  auto currentFirstChild = root->child;
  ReactRuntimeTestHelper::reconcileChildren(runtime, rt, root, currentFirstChild, jsi::Value(rt, newChildren));

  bool ok = true;

  if (root->deletions.size() != 1) {
    std::cerr << "Expected one deletion when removing keyed child" << std::endl;
    ok = false;
  } else if (root->deletions[0].get() != childA.get()) {
    std::cerr << "Expected deletion list to contain removed child 'a'" << std::endl;
    ok = false;
  }

  if (!hasFlag(root->flags, FiberFlags::ChildDeletion)) {
    std::cerr << "Expected ChildDeletion flag on root" << std::endl;
    ok = false;
  }

  auto firstChild = root->child;
  if (!firstChild) {
    std::cerr << "Expected first child after reconciliation" << std::endl;
    return false;
  }

  if (!firstChild->alternate || firstChild->alternate.get() != childB.get()) {
    std::cerr << "Expected keyed child 'b' to be reused" << std::endl;
    ok = false;
  }

  if (hasFlag(firstChild->flags, FiberFlags::Placement)) {
    std::cerr << "Did not expect Placement flag on reused child 'b'" << std::endl;
    ok = false;
  }

  auto secondChild = firstChild->sibling;
  if (!secondChild) {
    std::cerr << "Expected newly inserted child" << std::endl;
    return false;
  }

  if (secondChild->alternate) {
    std::cerr << "Expected new child 'c' not to have alternate" << std::endl;
    ok = false;
  }

  if (!hasFlag(secondChild->flags, FiberFlags::Placement)) {
    std::cerr << "Expected new child 'c' to carry Placement flag" << std::endl;
    ok = false;
  }

  if (secondChild->key.isUndefined() || !secondChild->key.isString() || secondChild->key.asString(rt).utf8(rt) != "c") {
    std::cerr << "Expected new child to carry key 'c'" << std::endl;
    ok = false;
  }

  return ok;
}

bool testHostInterfaceDomBridge(TestContext& ctx) {
  ctx.reactRuntime.reset();

  auto host = std::make_shared<RecordingHostInterface>();
  ctx.reactRuntime.setHostInterface(host);
  ctx.reactRuntime.bindHostInterface(ctx.runtime);

  auto& rt = ctx.runtime;

  jsi::Object parentProps(rt);
  parentProps.setProperty(rt, "id", jsi::String::createFromUtf8(rt, "root"));
  auto parent = ctx.reactRuntime.createInstance(rt, "div", parentProps);
  if (!parent) {
    std::cerr << "Expected parent instance to be created" << std::endl;
    return false;
  }

  auto text = ctx.reactRuntime.createTextInstance(rt, "hello");
  if (!text) {
    std::cerr << "Expected text instance to be created" << std::endl;
    return false;
  }
  ctx.reactRuntime.appendChild(parent, text);

  jsi::Object spanProps(rt);
  spanProps.setProperty(rt, "title", jsi::String::createFromUtf8(rt, "child"));
  auto span = ctx.reactRuntime.createInstance(rt, "span", spanProps);
  if (!span) {
    std::cerr << "Expected span instance to be created" << std::endl;
    return false;
  }
  ctx.reactRuntime.appendChild(parent, span);

  ctx.reactRuntime.insertBefore(parent, span, text);

  jsi::Object emptyProps(rt);
  jsi::Object payload(rt);
  payload.setProperty(rt, "text", jsi::String::createFromUtf8(rt, "world"));
  ctx.reactRuntime.commitUpdate(text, emptyProps, emptyProps, payload);

  ctx.reactRuntime.removeChild(parent, span);

  std::vector<std::string> expectedLog = {
    "create:div",
    "createText:hello",
    "append:#text",
    "create:span",
    "append:span",
    "insertBefore:span->#text",
    "commit:#text",
    "remove:span"
  };

  if (host->log.size() != expectedLog.size()) {
    std::cerr << "Expected host log length " << expectedLog.size() << " but got " << host->log.size() << std::endl;
    return false;
  }

  for (size_t i = 0; i < expectedLog.size(); ++i) {
    if (host->log[i] != expectedLog[i]) {
      std::cerr << "Host log mismatch at index " << i << ": expected '" << expectedLog[i]
                << "' but received '" << host->log[i] << "'" << std::endl;
      return false;
    }
  }

  auto parentComponent = std::dynamic_pointer_cast<ReactDOMComponent>(parent);
  if (!parentComponent) {
    std::cerr << "Expected parent to be ReactDOMComponent" << std::endl;
    return false;
  }

  if (parentComponent->children.size() != 1) {
    std::cerr << "Expected parent to contain exactly one child after removal" << std::endl;
    return false;
  }

  if (!parentComponent->children[0] || parentComponent->children[0].get() != text.get()) {
    std::cerr << "Expected remaining child to be text instance" << std::endl;
    return false;
  }

  auto textComponent = std::dynamic_pointer_cast<ReactDOMComponent>(text);
  if (!textComponent) {
    std::cerr << "Expected text instance to be ReactDOMComponent" << std::endl;
    return false;
  }

  if (textComponent->getTextContent() != "world") {
    std::cerr << "Expected commitUpdate to apply new text content" << std::endl;
    return false;
  }

  ctx.reactRuntime.setHostInterface(std::make_shared<HostInterface>());
  ctx.reactRuntime.bindHostInterface(rt);
  ctx.reactRuntime.reset();

  return true;
}

bool testSchedulerPriorityApi(TestContext&) {
  ReactRuntime runtime;

  SchedulerPriority observedPriority = SchedulerPriority::NoPriority;
  runtime.scheduleTask(SchedulerPriority::ImmediatePriority, [&]() {
    observedPriority = runtime.getCurrentPriorityLevel();
  });

  if (observedPriority != SchedulerPriority::ImmediatePriority) {
    std::cerr << "Expected scheduleTask to run with ImmediatePriority" << std::endl;
    return false;
  }

  bool executed = false;
  auto previous = runtime.runWithPriority(SchedulerPriority::UserBlockingPriority, [&]() {
    executed = true;
    if (runtime.getCurrentPriorityLevel() != SchedulerPriority::UserBlockingPriority) {
      std::cerr << "Expected runWithPriority to set current level" << std::endl;
    }
  });

  if (!executed) {
    std::cerr << "Expected runWithPriority callback to execute" << std::endl;
    return false;
  }

  if (previous != SchedulerPriority::ImmediatePriority && previous != SchedulerPriority::NormalPriority) {
    std::cerr << "Expected previous priority to be default level" << std::endl;
    return false;
  }

  if (runtime.getCurrentPriorityLevel() != previous) {
    std::cerr << "Expected current priority to restore after runWithPriority" << std::endl;
    return false;
  }

  return true;
}

} // namespace react

int main() {
  react::TestContext ctx;

  bool ok = true;
  ok &= react::testHostComponentPayload(ctx);
  ok &= react::testHostTextPayload(ctx);
  ok &= react::testCloneFiberSetsUpdate(ctx);
  ok &= react::testReactDOMComponentApplyUpdate(ctx);
  ok &= react::testReactDOMComponentDomApi(ctx);
  ok &= react::testCommitAppliesHostComponentUpdate(ctx);
  ok &= react::testSimpleHostTextUpdate(ctx);
  ok &= react::testRenderConvertsWasmLayout(ctx);
  ok &= react::testBridgeRenderRegistersRoot(ctx);
  ok &= react::testRenderRootKeyedDiffReuse(ctx);
  ok &= react::testRenderRootHandlesKeyedReorderAndDeletion(ctx);
  ok &= react::testReconcileArrayKeyedReuseAndUpdate(ctx);
  ok &= react::testReconcileArrayHandlesDeletionAndPlacement(ctx);
  ok &= react::testHostInterfaceDomBridge(ctx);
  ok &= react::testSchedulerPriorityApi(ctx);

  if (!ok) {
    std::cerr << "ReactRuntime tests failed" << std::endl;
    return 1;
  }

  std::cout << "ReactRuntime tests passed" << std::endl;
  return 0;
}
