#include "runtime/ReactJSXRuntime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace react::jsx {

namespace {
class ReactElementHostObject : public jsi::HostObject {
 public:
  explicit ReactElementHostObject(ReactElementPtr element)
      : element_(std::move(element)) {}

  ReactElementPtr element() const {
    return element_;
  }

  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime&) override {
    return {};
  }

  jsi::Value get(jsi::Runtime&, const jsi::PropNameID&) override {
    return jsi::Value::undefined();
  }

 private:
  ReactElementPtr element_;
};

jsi::Value makeHostValue(jsi::Runtime& runtime, const ReactElementPtr& element) {
  auto host = std::make_shared<ReactElementHostObject>(element);
  return jsi::Value(runtime, jsi::Object::createFromHostObject(runtime, host));
}

ReactElementPtr hostValueToElement(jsi::Runtime& runtime, const jsi::Value& value) {
  if (!value.isObject()) {
    return nullptr;
  }
  auto object = value.getObject(runtime);
  if (!object.isHostObject(runtime)) {
    return nullptr;
  }
  auto host = runtime.getHostObject(object);
  if (!host) {
    return nullptr;
  }
  auto typed = std::dynamic_pointer_cast<ReactElementHostObject>(host);
  if (!typed) {
    return nullptr;
  }
  return typed->element();
}

std::string numberToString(double number) {
  if (!std::isfinite(number)) {
    throw std::invalid_argument("Cannot convert non-finite number to string");
  }
  std::ostringstream out;
  out.setf(std::ios::fmtflags(0), std::ios::floatfield);
  out << number;
  return out.str();
}

std::string coerceToString(jsi::Runtime& runtime, const jsi::Value& value) {
  if (value.isString()) {
    return value.getString(runtime).utf8(runtime);
  }
  if (value.isNumber()) {
    return numberToString(value.getNumber());
  }
  throw std::invalid_argument("Value cannot be converted to string");
}

std::optional<jsi::Value> takeProp(PropList& props, const std::string& name) {
  for (auto it = props.begin(); it != props.end(); ++it) {
    if (it->first == name) {
      auto value = it->second;
      props.erase(it);
      return value;
    }
  }
  return std::nullopt;
}

void removeReservedProps(PropList& props) {
  static constexpr std::array kReserved{"__self", "__source"};
  props.erase(
      std::remove_if(
          props.begin(),
          props.end(),
          [](const PropEntry& entry) {
            return std::find(kReserved.begin(), kReserved.end(), entry.first) != kReserved.end();
          }),
      props.end());
}

Value convertPropValue(jsi::Runtime& runtime, const jsi::Value& value) {
  if (value.isNull()) {
    return Value::null();
  }
  if (value.isUndefined()) {
    return Value::undefined();
  }
  if (value.isBool()) {
    return Value::boolean(value.getBool());
  }
  if (value.isNumber()) {
    return Value::number(value.getNumber());
  }
  if (value.isString()) {
    return Value::string(value.getString(runtime).utf8(runtime));
  }
  throw std::invalid_argument("Unsupported prop value type; expected string, number, or boolean");
}

void collectChildrenRecursive(jsi::Runtime& runtime, const jsi::Value& value, std::vector<Value>& out) {
  if (value.isUndefined() || value.isNull()) {
    return;
  }
  if (value.isBool()) {
    return;
  }
  if (value.isNumber()) {
    out.push_back(Value::number(value.getNumber()));
    return;
  }
  if (value.isString()) {
    out.push_back(Value::string(value.getString(runtime).utf8(runtime)));
    return;
  }
  if (value.isObject()) {
    if (auto element = hostValueToElement(runtime, value)) {
      out.push_back(Value::element(element));
      return;
    }

    auto object = value.getObject(runtime);
    if (object.isArray(runtime)) {
      auto array = object.getArray(runtime);
      const auto length = array.size(runtime);
      for (size_t index = 0; index < length; ++index) {
        collectChildrenRecursive(runtime, array.getValueAtIndex(runtime, index), out);
      }
      return;
    }
  }

  throw std::invalid_argument("Unsupported child node in JSX runtime");
}

std::vector<Value> extractChildren(jsi::Runtime& runtime, PropList& props) {
  std::vector<Value> children;
  for (auto it = props.begin(); it != props.end();) {
    if (it->first == "children") {
      collectChildrenRecursive(runtime, it->second, children);
      it = props.erase(it);
    } else {
      ++it;
    }
  }
  return children;
}

ReactElementPtr createElement(
    jsi::Runtime& runtime,
    const jsi::Value& type,
    PropList props,
    std::optional<jsi::Value> key,
    std::optional<jsi::Value> ref,
    std::optional<SourceLocation> source) {
  auto element = std::make_shared<ReactElement>();
  element->type = jsi::Value(runtime, type);
  if (type.isString()) {
    element->hostType = type.getString(runtime).utf8(runtime);
  }
  if (key) {
    coerceToString(runtime, *key);
    element->key = *key;
  }
  if (ref) {
    element->ref = *ref;
  }
  element->children = extractChildren(runtime, props);
  removeReservedProps(props);
  element->props = std::move(props);
  element->source = std::move(source);
  return element;
}

struct WasmMemoryBuilder {
  WasmMemoryBuilder() {
    buffer.push_back(0);
  }

  template <typename T>
  uint32_t appendStruct(const T& value) {
    const auto offset = static_cast<uint32_t>(buffer.size());
    const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
    return offset;
  }

  uint32_t appendProps(const std::vector<WasmReactProp>& props) {
    if (props.empty()) {
      return 0;
    }
    uint32_t firstOffset = 0;
    for (size_t index = 0; index < props.size(); ++index) {
      const auto offset = appendStruct(props[index]);
      if (index == 0) {
        firstOffset = offset;
      }
    }
    return firstOffset;
  }

  uint32_t appendValues(const std::vector<WasmReactValue>& values) {
    if (values.empty()) {
      return 0;
    }
    uint32_t firstOffset = 0;
    for (size_t index = 0; index < values.size(); ++index) {
      const auto offset = appendStruct(values[index]);
      if (index == 0) {
        firstOffset = offset;
      }
    }
    return firstOffset;
  }

  uint32_t internString(const std::string& value) {
    const auto it = stringOffsets.find(value);
    if (it != stringOffsets.end()) {
      return it->second;
    }
    const auto offset = static_cast<uint32_t>(buffer.size());
    buffer.insert(buffer.end(), value.begin(), value.end());
    buffer.push_back('\0');
    stringOffsets.emplace(value, offset);
    return offset;
  }

  std::vector<uint8_t> takeBuffer() {
    return std::move(buffer);
  }

  std::vector<uint8_t> buffer;
  std::unordered_map<std::string, uint32_t> stringOffsets;
};

WasmReactValue encodeValue(jsi::Runtime& runtime, const Value& value, WasmMemoryBuilder& builder);
uint32_t encodeElement(jsi::Runtime& runtime, const ReactElement& element, WasmMemoryBuilder& builder);

std::vector<WasmReactProp> encodeProps(jsi::Runtime& runtime, const PropList& props, WasmMemoryBuilder& builder) {
  std::vector<WasmReactProp> encoded;
  encoded.reserve(props.size());
  for (const auto& [name, jsValue] : props) {
    auto converted = convertPropValue(runtime, jsValue);
    if (converted.kind == ValueKind::Null || converted.kind == ValueKind::Undefined) {
      continue;
    }
    if (converted.kind != ValueKind::String && converted.kind != ValueKind::Number && converted.kind != ValueKind::Boolean) {
      throw std::invalid_argument("Unsupported prop value while encoding");
    }

    WasmReactProp prop{};
    prop.key_ptr = builder.internString(name);
    prop.value = encodeValue(runtime, converted, builder);
    encoded.push_back(prop);
  }
  return encoded;
}

WasmReactValue encodeValue(jsi::Runtime& runtime, const Value& value, WasmMemoryBuilder& builder) {
  WasmReactValue encoded{};
  switch (value.kind) {
    case ValueKind::Null:
      encoded.type = WasmValueType::Null;
      encoded.data.ptrValue = 0;
      return encoded;
    case ValueKind::Undefined:
      encoded.type = WasmValueType::Undefined;
      encoded.data.ptrValue = 0;
      return encoded;
    case ValueKind::Boolean:
      encoded.type = WasmValueType::Boolean;
      encoded.data.boolValue = std::get<bool>(value.payload);
      return encoded;
    case ValueKind::Number:
      encoded.type = WasmValueType::Number;
      encoded.data.numberValue = std::get<double>(value.payload);
      return encoded;
    case ValueKind::String:
      encoded.type = WasmValueType::String;
      encoded.data.ptrValue = builder.internString(std::get<std::string>(value.payload));
      return encoded;
    case ValueKind::Element: {
      encoded.type = WasmValueType::Element;
      encoded.data.ptrValue = encodeElement(runtime, *std::get<ReactElementPtr>(value.payload), builder);
      return encoded;
    }
    case ValueKind::Array: {
      const auto& nested = std::get<std::vector<Value>>(value.payload);
      std::vector<WasmReactValue> items;
      items.reserve(nested.size());
      for (const auto& entry : nested) {
        items.push_back(encodeValue(runtime, entry, builder));
      }
      WasmReactArray array{};
      array.length = static_cast<uint32_t>(nested.size());
      array.items_ptr = builder.appendValues(items);
      encoded.type = WasmValueType::Array;
      encoded.data.ptrValue = builder.appendStruct(array);
      return encoded;
    }
  }
  throw std::invalid_argument("Unhandled value kind during encoding");
}

uint32_t encodeElement(jsi::Runtime& runtime, const ReactElement& element, WasmMemoryBuilder& builder) {
  if (!element.hostType) {
    throw std::invalid_argument("JSX runtime can only serialize host elements identified by string type");
  }
  const auto typeOffset = builder.internString(*element.hostType);
  uint32_t keyOffset = 0;
  if (element.key) {
    keyOffset = builder.internString(coerceToString(runtime, *element.key));
  }

  const auto props = encodeProps(runtime, element.props, builder);

  std::vector<WasmReactValue> childValues;
  childValues.reserve(element.children.size());
  for (const auto& child : element.children) {
    childValues.push_back(encodeValue(runtime, child, builder));
  }

  const auto propsOffset = builder.appendProps(props);
  const auto childrenOffset = builder.appendValues(childValues);

  WasmReactElement encoded{};
  encoded.type_name_ptr = typeOffset;
  encoded.key_ptr = keyOffset;
  encoded.ref_ptr = 0;
  encoded.props_count = static_cast<uint32_t>(props.size());
  encoded.props_ptr = propsOffset;
  encoded.children_count = static_cast<uint32_t>(childValues.size());
  encoded.children_ptr = childrenOffset;

  return builder.appendStruct(encoded);
}

} // namespace

Value Value::null() {
  Value result;
  result.kind = ValueKind::Null;
  result.payload = std::monostate{};
  return result;
}

Value Value::undefined() {
  Value result;
  result.kind = ValueKind::Undefined;
  result.payload = std::monostate{};
  return result;
}

Value Value::boolean(bool value) {
  Value result;
  result.kind = ValueKind::Boolean;
  result.payload = value;
  return result;
}

Value Value::number(double value) {
  Value result;
  result.kind = ValueKind::Number;
  result.payload = value;
  return result;
}

Value Value::string(std::string value) {
  Value result;
  result.kind = ValueKind::String;
  result.payload = std::move(value);
  return result;
}

Value Value::element(const ReactElementPtr& value) {
  Value result;
  result.kind = ValueKind::Element;
  result.payload = value;
  return result;
}

Value Value::array(std::vector<Value> values) {
  Value result;
  result.kind = ValueKind::Array;
  result.payload = std::move(values);
  return result;
}

bool Value::isTruthyRenderable() const {
  switch (kind) {
    case ValueKind::Null:
    case ValueKind::Undefined:
    case ValueKind::Boolean:
      return false;
    case ValueKind::String:
    case ValueKind::Number:
    case ValueKind::Element:
    case ValueKind::Array:
      return true;
  }
  return false;
}

ReactElementPtr jsx(
    jsi::Runtime& runtime,
  const jsi::Value& type,
    PropList props,
    std::optional<jsi::Value> key,
    std::optional<jsi::Value> ref) {
  return createElement(runtime, type, std::move(props), std::move(key), std::move(ref), std::nullopt);
}

ReactElementPtr jsxs(
    jsi::Runtime& runtime,
  const jsi::Value& type,
    PropList props,
    std::optional<jsi::Value> key,
    std::optional<jsi::Value> ref) {
  return createElement(runtime, type, std::move(props), std::move(key), std::move(ref), std::nullopt);
}

ReactElementPtr jsxDEV(
    jsi::Runtime& runtime,
  const jsi::Value& type,
    PropList config,
    std::optional<jsi::Value> maybeKey,
    SourceLocation source,
    std::optional<jsi::Value> ref) {
  auto cloneOptional = [&runtime](const std::optional<jsi::Value>& input) -> std::optional<jsi::Value> {
    if (!input) {
      return std::nullopt;
    }
    return std::optional<jsi::Value>(jsi::Value(runtime, *input));
  };

  auto keyValue = cloneOptional(maybeKey);
  if (!keyValue) {
    auto extracted = takeProp(config, "key");
    keyValue = cloneOptional(extracted);
  } else {
    takeProp(config, "key");
  }
  if (keyValue) {
    coerceToString(runtime, *keyValue);
  }

  auto refValue = cloneOptional(ref);
  if (!refValue) {
    auto extractedRef = takeProp(config, "ref");
    refValue = cloneOptional(extractedRef);
  } else {
    takeProp(config, "ref");
  }

  std::optional<SourceLocation> location;
  if (source.isValid()) {
    location = source;
  }

  return createElement(runtime, type, std::move(config), std::move(keyValue), std::move(refValue), std::move(location));
}

WasmSerializedLayout serializeToWasm(jsi::Runtime& runtime, const ReactElement& element) {
  WasmMemoryBuilder builder;
  const auto rootOffset = encodeElement(runtime, element, builder);
  WasmSerializedLayout layout;
  layout.buffer = builder.takeBuffer();
  layout.rootOffset = rootOffset;
  return layout;
}

jsi::Value createJsxHostValue(jsi::Runtime& runtime, const ReactElementPtr& element) {
  return makeHostValue(runtime, element);
}

ReactElementPtr getReactElementFromValue(jsi::Runtime& runtime, const jsi::Value& value) {
  return hostValueToElement(runtime, value);
}

bool isReactElementValue(jsi::Runtime& runtime, const jsi::Value& value) {
  return hostValueToElement(runtime, value) != nullptr;
}

} // namespace react::jsx
