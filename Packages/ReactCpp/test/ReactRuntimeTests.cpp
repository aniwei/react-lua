#include "runtime/ReactRuntime.h"
#include "runtime/ReactWasmBridge.h"
#include "runtime/ReactWasmLayout.h"
#include "host/SimpleHostInstance.h"
#include "reconciler/FiberNode.h"

#include "jsi/jsi.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace facebook::jsi;

namespace react {

namespace {

class TestRuntime : public Runtime {
 public:
  TestRuntime() = default;

  Array makeArray(size_t length) {
    return createArray(length);
  }

  ~TestRuntime() override = default;

  Value evaluateJavaScript(
      const std::shared_ptr<const Buffer>&,
      const std::string&) override {
    throw JSINativeException("evaluateJavaScript not implemented");
  }

  std::shared_ptr<const PreparedJavaScript> prepareJavaScript(
      const std::shared_ptr<const Buffer>&,
      std::string) override {
    throw JSINativeException("prepareJavaScript not implemented");
  }

  Value evaluatePreparedJavaScript(
      const std::shared_ptr<const PreparedJavaScript>&) override {
    throw JSINativeException("evaluatePreparedJavaScript not implemented");
  }

  void queueMicrotask(const Function&) override {}

  bool drainMicrotasks(int = -1) override {
    return true;
  }

  Object global() override {
    return createObject();
  }

  std::string description() override {
    return "TestRuntime";
  }

  bool isInspectable() override {
    return false;
  }

 protected:
  void setRuntimeDataImpl(
      const UUID&,
      const void*,
      void (*)(const void*)) override {}

  const void* getRuntimeDataImpl(const UUID&) override {
    return nullptr;
  }

  PointerValue* cloneSymbol(const PointerValue*) override {
    throw JSINativeException("Symbols not supported in TestRuntime");
  }

  PointerValue* cloneBigInt(const PointerValue*) override {
    throw JSINativeException("BigInt not supported in TestRuntime");
  }

  PointerValue* cloneString(const PointerValue* pv) override {
    const auto* stringValue = static_cast<const StringValue*>(pv);
    if (!stringValue) {
      return nullptr;
    }
    return new StringValue(stringValue->data);
  }

  PointerValue* cloneObject(const PointerValue* pv) override {
    const auto* objectValue = static_cast<const ObjectValue*>(pv);
    if (!objectValue) {
      return nullptr;
    }
    return new ObjectValue(objectValue->data);
  }

  PointerValue* clonePropNameID(const PointerValue* pv) override {
    const auto* propValue = static_cast<const PropNameIDValue*>(pv);
    if (!propValue) {
      return nullptr;
    }
    return new PropNameIDValue(propValue->name);
  }

  PropNameID createPropNameIDFromAscii(const char* str, size_t length) override {
    return createPropNameIDFromUtf8(reinterpret_cast<const uint8_t*>(str), length);
  }

  PropNameID createPropNameIDFromUtf8(const uint8_t* utf8, size_t length) override {
    auto name = std::make_shared<std::string>(reinterpret_cast<const char*>(utf8), length);
    return make<PropNameID>(new PropNameIDValue(std::move(name)));
  }

  PropNameID createPropNameIDFromString(const String& str) override {
    return createPropNameIDFromUtf8(reinterpret_cast<const uint8_t*>(utf8(str).data()), utf8(str).size());
  }

  PropNameID createPropNameIDFromSymbol(const Symbol&) override {
    throw JSINativeException("Symbols not supported in TestRuntime");
  }

  std::string utf8(const PropNameID& name) override {
    const auto* value = static_cast<const PropNameIDValue*>(Runtime::getPointerValue(name));
    if (!value || !value->name) {
      return std::string();
    }
    return *value->name;
  }

  bool compare(const PropNameID& lhs, const PropNameID& rhs) override {
    return utf8(lhs) == utf8(rhs);
  }

  std::string symbolToString(const Symbol&) override {
    throw JSINativeException("Symbols not supported in TestRuntime");
  }

  BigInt createBigIntFromInt64(int64_t) override {
    throw JSINativeException("BigInt not supported in TestRuntime");
  }

  BigInt createBigIntFromUint64(uint64_t) override {
    throw JSINativeException("BigInt not supported in TestRuntime");
  }

  bool bigintIsInt64(const BigInt&) override {
    throw JSINativeException("BigInt not supported in TestRuntime");
  }

  bool bigintIsUint64(const BigInt&) override {
    throw JSINativeException("BigInt not supported in TestRuntime");
  }

  uint64_t truncate(const BigInt&) override {
    throw JSINativeException("BigInt not supported in TestRuntime");
  }

  String bigintToString(const BigInt&, int) override {
    throw JSINativeException("BigInt not supported in TestRuntime");
  }

  String createStringFromAscii(const char* str, size_t length) override {
    return createStringFromUtf8(reinterpret_cast<const uint8_t*>(str), length);
  }

  String createStringFromUtf8(const uint8_t* utf8, size_t length) override {
    auto stringValue = std::make_shared<std::string>(reinterpret_cast<const char*>(utf8), length);
    return make<String>(new StringValue(std::move(stringValue)));
  }

  std::string utf8(const String& str) override {
    const auto* value = static_cast<const StringValue*>(Runtime::getPointerValue(str));
    if (!value || !value->data) {
      return std::string();
    }
    return *value->data;
  }

  Object createObject() override {
    auto data = std::make_shared<ObjectData>();
    return make<Object>(new ObjectValue(std::move(data)));
  }

  Object createObject(std::shared_ptr<HostObject> hostObject) override {
    auto data = std::make_shared<ObjectData>();
    data->hostObject = std::move(hostObject);
    return make<Object>(new ObjectValue(std::move(data)));
  }

  std::shared_ptr<HostObject> getHostObject(const Object& obj) override {
    auto data = getObjectData(obj);
    return data ? data->hostObject : nullptr;
  }

  HostFunctionType& getHostFunction(const Function&) override {
    throw JSINativeException("Host functions not supported in TestRuntime");
  }

  bool hasNativeState(const Object& obj) override {
    auto data = getObjectData(obj);
    return data && data->nativeState != nullptr;
  }

  std::shared_ptr<NativeState> getNativeState(const Object& obj) override {
    auto data = getObjectData(obj);
    return data ? data->nativeState : nullptr;
  }

  void setNativeState(
      const Object& obj,
      std::shared_ptr<NativeState> state) override {
    auto data = getObjectData(obj);
    if (data) {
      data->nativeState = std::move(state);
    }
  }

  Value getProperty(const Object& obj, const PropNameID& name) override {
    return getProperty(obj, utf8(name));
  }

  Value getProperty(const Object& obj, const String& name) override {
    return getProperty(obj, utf8(name));
  }

  Value getProperty(const Object& obj, const Value& nameValue) override {
    if (nameValue.isString()) {
      auto& base = baseRuntime();
      return getProperty(obj, nameValue.getString(base).utf8(base));
    }
    return Value::undefined();
  }

  bool hasProperty(const Object& obj, const PropNameID& name) override {
    return hasProperty(obj, utf8(name));
  }

  bool hasProperty(const Object& obj, const String& name) override {
    return hasProperty(obj, utf8(name));
  }

  bool hasProperty(const Object& obj, const Value& nameValue) override {
    if (nameValue.isString()) {
      auto& base = baseRuntime();
      return hasProperty(obj, nameValue.getString(base).utf8(base));
    }
    return false;
  }

  void setPropertyValue(
      const Object& object,
      const PropNameID& name,
      const Value& value) override {
    setProperty(object, utf8(name), value);
  }

  void setPropertyValue(
      const Object& object,
      const String& name,
      const Value& value) override {
    setProperty(object, utf8(name), value);
  }

  void setPropertyValue(
      const Object& object,
      const Value& nameValue,
      const Value& value) override {
    if (nameValue.isString()) {
      auto& base = baseRuntime();
      setProperty(object, nameValue.getString(base).utf8(base), value);
    }
  }

  void deleteProperty(const Object& object, const PropNameID& name) override {
    deleteProperty(object, utf8(name));
  }

  void deleteProperty(const Object& object, const String& name) override {
    deleteProperty(object, utf8(name));
  }

  void deleteProperty(const Object& object, const Value& nameValue) override {
    if (nameValue.isString()) {
      auto& base = baseRuntime();
      deleteProperty(object, nameValue.getString(base).utf8(base));
    }
  }

  bool isArray(const Object& object) const override {
    auto data = getObjectData(object);
    return data && data->isArray;
  }

  bool isArrayBuffer(const Object&) const override {
    return false;
  }

  bool isFunction(const Object&) const override {
    return false;
  }

  bool isHostObject(const Object& object) const override {
    auto data = getObjectData(object);
    return data && data->hostObject != nullptr;
  }

  bool isHostFunction(const Function&) const override {
    return false;
  }

  Array getPropertyNames(const Object& object) override {
    auto data = getObjectData(object);
    if (!data) {
      return createArray(0);
    }
  auto& base = baseRuntime();
    Array array = createArray(data->props.size());
    size_t index = 0;
    for (const auto& entry : data->props) {
      auto key = String::createFromUtf8(base, entry.first);
      setValueAtIndexImpl(array, index++, Value(base, key));
    }
    return array;
  }

  WeakObject createWeakObject(const Object& object) override {
    auto data = getObjectData(object);
    auto weakValue = std::make_shared<WeakObjectValue>();
    weakValue->data = data;
    return make<WeakObject>(new WeakObjectValue(*weakValue));
  }

  Value lockWeakObject(const WeakObject& weak) override {
    const auto* value = static_cast<const WeakObjectValue*>(Runtime::getPointerValue(weak));
    if (!value) {
      return Value::undefined();
    }
    auto locked = value->data.lock();
    if (!locked) {
      return Value::undefined();
    }
  auto& base = baseRuntime();
    auto object = make<Object>(new ObjectValue(std::move(locked)));
    return Value(base, object);
  }

  Array createArray(size_t length) override {
    auto data = std::make_shared<ObjectData>();
    data->isArray = true;
    data->elements.resize(length);
    return make<Array>(new ObjectValue(std::move(data)));
  }

  ArrayBuffer createArrayBuffer(std::shared_ptr<MutableBuffer>) override {
    throw JSINativeException("ArrayBuffer not supported in TestRuntime");
  }

  size_t size(const Array& array) override {
    auto data = getObjectData(array);
    return data ? data->elements.size() : 0;
  }

  size_t size(const ArrayBuffer&) override {
    throw JSINativeException("ArrayBuffer not supported in TestRuntime");
  }

  uint8_t* data(const ArrayBuffer&) override {
    throw JSINativeException("ArrayBuffer not supported in TestRuntime");
  }

  Value getValueAtIndex(const Array& array, size_t index) override {
    auto data = getObjectData(array);
    if (!data || index >= data->elements.size()) {
      return Value::undefined();
    }
    auto stored = data->elements[index];
    if (!stored) {
      return Value::undefined();
    }
    return Value(baseRuntime(), *stored);
  }

  void setValueAtIndexImpl(
      const Array& array,
      size_t index,
      const Value& value) override {
    auto data = getObjectData(array);
    if (!data) {
      return;
    }
    if (index >= data->elements.size()) {
      data->elements.resize(index + 1);
    }
    data->elements[index] = cloneValuePtr(value);
  }

  Function createFunctionFromHostFunction(
      const PropNameID&,
      unsigned int,
      HostFunctionType) override {
    throw JSINativeException("Host functions not supported in TestRuntime");
  }

  Value call(
      const Function&,
      const Value&,
      const Value*,
      size_t) override {
    throw JSINativeException("Function call not supported in TestRuntime");
  }

  Value callAsConstructor(
      const Function&,
      const Value*,
      size_t) override {
    throw JSINativeException("Function call not supported in TestRuntime");
  }

  ScopeState* pushScope() override {
    return nullptr;
  }

  void popScope(ScopeState*) override {}

  bool strictEquals(const Symbol&, const Symbol&) const override {
    throw JSINativeException("Symbols not supported in TestRuntime");
  }

  bool strictEquals(const BigInt&, const BigInt&) const override {
    throw JSINativeException("BigInt not supported in TestRuntime");
  }

  bool strictEquals(const String& a, const String& b) const override {
    const auto* valueA = static_cast<const StringValue*>(Runtime::getPointerValue(a));
    const auto* valueB = static_cast<const StringValue*>(Runtime::getPointerValue(b));
    if (!valueA || !valueB || !valueA->data || !valueB->data) {
      return valueA == valueB;
    }
    return *valueA->data == *valueB->data;
  }

  bool strictEquals(const Object& a, const Object& b) const override {
    auto dataA = getObjectData(a);
    auto dataB = getObjectData(b);
    return dataA == dataB;
  }

  bool instanceOf(const Object&, const Function&) override {
    return false;
  }

  void setExternalMemoryPressure(const Object&, size_t) override {}

  void getStringData(
      const String& str,
      void* ctx,
      void (*cb)(void* ctx, bool ascii, const void* data, size_t num)) override {
    auto value = utf8(str);
    cb(ctx, true, value.data(), value.size());
  }

  void getPropNameIdData(
      const PropNameID& sym,
      void* ctx,
      void (*cb)(void* ctx, bool ascii, const void* data, size_t num)) override {
    auto value = utf8(sym);
    cb(ctx, true, value.data(), value.size());
  }

 private:
    Runtime& baseRuntime() {
      return static_cast<Runtime&>(*this);
    }

    const Runtime& baseRuntime() const {
      return static_cast<const Runtime&>(*this);
    }

    Runtime& mutableBaseRuntime() const {
      return const_cast<TestRuntime*>(this)->baseRuntime();
    }

    std::shared_ptr<Value> cloneValuePtr(const Value& value) {
      return std::make_shared<Value>(baseRuntime(), value);
    }

    std::shared_ptr<Value> cloneValuePtr(const Value& value) const {
      return std::make_shared<Value>(mutableBaseRuntime(), value);
    }

  struct StringValue : PointerValue {
    explicit StringValue(std::shared_ptr<std::string> data) : data(std::move(data)) {}
    void invalidate() noexcept override {
      data.reset();
    }
    std::shared_ptr<std::string> data;
  };

  struct PropNameIDValue : PointerValue {
    explicit PropNameIDValue(std::shared_ptr<std::string> name) : name(std::move(name)) {}
    void invalidate() noexcept override {
      name.reset();
    }
    std::shared_ptr<std::string> name;
  };

  struct ObjectData {
    std::unordered_map<std::string, std::shared_ptr<Value>> props;
    std::shared_ptr<HostObject> hostObject;
    std::shared_ptr<NativeState> nativeState;
    bool isArray{false};
    std::vector<std::shared_ptr<Value>> elements;
  };

  struct ObjectValue : PointerValue {
    explicit ObjectValue(std::shared_ptr<ObjectData> data) : data(std::move(data)) {}
    void invalidate() noexcept override {
      data.reset();
    }
    std::shared_ptr<ObjectData> data;
  };

  struct WeakObjectValue : PointerValue {
    WeakObjectValue() = default;
    explicit WeakObjectValue(std::weak_ptr<ObjectData> weak) : data(std::move(weak)) {}
    void invalidate() noexcept override {
      data.reset();
    }
    std::weak_ptr<ObjectData> data;
  };

  std::shared_ptr<ObjectData> getObjectData(const Object& object) const {
    const auto* value = static_cast<const ObjectValue*>(Runtime::getPointerValue(object));
    return value ? value->data : nullptr;
  }

  std::shared_ptr<ObjectData> getObjectData(const Array& array) const {
    const auto* value = static_cast<const ObjectValue*>(Runtime::getPointerValue(array));
    return value ? value->data : nullptr;
  }

  std::shared_ptr<ObjectData> getObjectData(const Value& value) const {
    if (!value.isObject()) {
      return nullptr;
    }
    return getObjectData(value.getObject(mutableBaseRuntime()));
  }

  void setProperty(const Object& object, const std::string& name, const Value& value) {
    auto data = getObjectData(object);
    if (!data) {
      return;
    }
    data->props[name] = cloneValuePtr(value);
  }

  void deleteProperty(const Object& object, const std::string& name) {
    auto data = getObjectData(object);
    if (!data) {
      return;
    }
    data->props.erase(name);
  }

  bool hasProperty(const Object& object, const std::string& name) {
    auto data = getObjectData(object);
    if (!data) {
      return false;
    }
    return data->props.find(name) != data->props.end();
  }

  Value getProperty(const Object& object, const std::string& name) {
    auto data = getObjectData(object);
    if (!data) {
      return Value::undefined();
    }
    auto it = data->props.find(name);
    if (it == data->props.end()) {
      return Value::undefined();
    }
    if (!it->second) {
      return Value::undefined();
    }
    return Value(baseRuntime(), *it->second);
  }

};

} // namespace

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

bool hasFlag(FiberFlags value, FiberFlags flag) {
  return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

// Helper for test assertions.
struct TestContext {
  TestRuntime runtime;
  ReactRuntime reactRuntime;
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
  auto keys = payloadObj.getPropertyNames(rt);
  size_t actualSize = keys.size(rt);
  if (actualSize != 3) {
    std::cerr << "Expected payload to contain three entries (including removals), got " << actualSize << std::endl;
    for (size_t i = 0; i < actualSize; ++i) {
      auto name = keys.getValueAtIndex(rt, i);
      if (name.isString()) {
        std::cerr << "  key[" << i << "]= " << name.asString(rt).utf8(rt) << std::endl;
      }
    }
    return false;
  }

  auto classNameValue = payloadObj.getProperty(rt, "className");
  if (!classNameValue.isString() || classNameValue.asString(rt).utf8(rt) != "new") {
    std::cerr << "Expected className to be updated" << std::endl;
    return false;
  }

  auto titleValue = payloadObj.getProperty(rt, "title");
  if (!titleValue.isString() || titleValue.asString(rt).utf8(rt) != "hello") {
    std::cerr << "Expected title to be added" << std::endl;
    return false;
  }

  auto removedValue = payloadObj.getProperty(rt, "data-old");
  if (!removedValue.isUndefined()) {
    std::cerr << "Expected removed prop to be undefined" << std::endl;
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
  auto classNameValue = payloadObj.getProperty(rt, "className");
  if (!classNameValue.isString() || classNameValue.asString(rt).utf8(rt) != "new") {
    std::cerr << "Expected payload to contain new className" << std::endl;
    return false;
  }

  return true;
}

bool testSimpleHostInstanceApplyUpdate(TestContext& ctx) {
  auto& rt = ctx.runtime;

  Object initialProps(rt);
  initialProps.setProperty(rt, "className", String::createFromUtf8(rt, "old"));
  initialProps.setProperty(rt, "title", String::createFromUtf8(rt, "hello"));

  SimpleHostInstance instance(rt, "div", initialProps);

  Object newProps(rt);
  newProps.setProperty(rt, "className", String::createFromUtf8(rt, "new"));

  Object payload(rt);
  payload.setProperty(rt, "className", String::createFromUtf8(rt, "new"));
  payload.setProperty(rt, "title", Value::undefined());

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

bool testCommitAppliesHostComponentUpdate(TestContext& ctx) {
  auto& rt = ctx.runtime;
  auto& runtime = ctx.reactRuntime;

  Object rootProps(rt);
  auto rootInstance = std::make_shared<SimpleHostInstance>(rt, "root", rootProps);

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

  auto hostInstance = std::make_shared<SimpleHostInstance>(rt, "div", prevProps);
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

  SimpleHostInstance textInstance(rt, "#text", "hello");
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
  auto rootContainer = std::make_shared<SimpleHostInstance>(rt, "root", rootProps);

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

} // namespace react

int main() {
  react::TestContext ctx;

  bool ok = true;
  ok &= react::testHostComponentPayload(ctx);
  ok &= react::testHostTextPayload(ctx);
  ok &= react::testCloneFiberSetsUpdate(ctx);
  ok &= react::testSimpleHostInstanceApplyUpdate(ctx);
  ok &= react::testCommitAppliesHostComponentUpdate(ctx);
  ok &= react::testSimpleHostTextUpdate(ctx);
  ok &= react::testRenderConvertsWasmLayout(ctx);
  ok &= react::testBridgeRenderRegistersRoot(ctx);
  ok &= react::testReconcileArrayKeyedReuseAndUpdate(ctx);
  ok &= react::testReconcileArrayHandlesDeletionAndPlacement(ctx);

  if (!ok) {
    std::cerr << "ReactRuntime tests failed" << std::endl;
    return 1;
  }

  std::cout << "ReactRuntime tests passed" << std::endl;
  return 0;
}
