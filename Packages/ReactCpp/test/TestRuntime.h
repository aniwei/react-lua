#pragma once

#include "jsi/jsi.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace react::test {

using namespace facebook::jsi;

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

} // namespace react::test
