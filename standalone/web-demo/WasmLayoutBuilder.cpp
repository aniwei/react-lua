#include "WasmLayoutBuilder.h"

#include <cstring>

namespace react::browser {

namespace {

struct WasmMemoryBuilder {
  WasmMemoryBuilder() {
    buffer.push_back(0); // Reserve offset 0 as null sentinel.
  }

  uint32_t appendBytes(const void* data, size_t size) {
    uint32_t offset = static_cast<uint32_t>(buffer.size());
    const auto* bytes = static_cast<const uint8_t*>(data);
    buffer.insert(buffer.end(), bytes, bytes + size);
    return offset;
  }

  uint32_t appendString(const std::string& value) {
    uint32_t offset = static_cast<uint32_t>(buffer.size());
    buffer.insert(buffer.end(), value.begin(), value.end());
    buffer.push_back('\0');
    return offset;
  }

  template <typename T>
  uint32_t appendStruct(const T& value) {
    return appendBytes(&value, sizeof(T));
  }

  uint8_t* data() {
    return buffer.data();
  }

  std::vector<uint8_t> buffer;
};

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

} // namespace

WasmLayout buildKeyedSpanLayout(const std::vector<std::pair<std::string, std::string>>& childrenSpecs) {
  WasmMemoryBuilder builder;

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
  const uint32_t rootOffset = builder.appendStruct(root);

  WasmLayout layout;
  layout.buffer = std::move(builder.buffer);
  layout.rootOffset = rootOffset;
  return layout;
}

} // namespace react::browser
