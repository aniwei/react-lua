#include "HostInstance.h"

namespace react {

facebook::jsi::Value HostInstance::get(
  facebook::jsi::Runtime& rt, 
  const facebook::jsi::PropNameID& name) {
  // This is a simplified implementation.
  // A real implementation would handle properties like `children`, `className`, etc.
  auto nameStr = name.utf8(rt);
  if (nameStr == "tagName") {
    return facebook::jsi::String::createFromUtf8(rt, tagName);
  }
  // Return undefined for unhandled properties
  return facebook::jsi::Value::undefined();
}

void HostInstance::set(
  facebook::jsi::Runtime& rt, 
  const facebook::jsi::PropNameID& name, 
  const facebook::jsi::Value& value) {
  // This is a simplified implementation.
  // A real implementation would handle properties like `className`, `style`, etc.
  auto nameStr = name.utf8(rt);
  if (nameStr == "className" && value.isString()) {
    className = value.asString(rt).utf8(rt);
  }
}

std::vector<facebook::jsi::PropNameID> HostInstance::getPropertyNames(facebook::jsi::Runtime& rt) {
  // A real implementation would return all valid properties.
  std::vector<facebook::jsi::PropNameID> props;
  props.push_back(facebook::jsi::PropNameID::forUtf8(rt, "tagName"));
  props.push_back(facebook::jsi::PropNameID::forUtf8(rt, "className"));
  props.push_back(facebook::jsi::PropNameID::forUtf8(rt, "children"));
  return props;
}

} // namespace react
