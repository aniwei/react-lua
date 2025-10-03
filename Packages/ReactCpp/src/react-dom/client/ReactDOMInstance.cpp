#include "ReactDOMInstance.h"

namespace react {

void ReactDOMInstance::setKey(std::string keyValue) {
  key = std::move(keyValue);
}

const std::string& ReactDOMInstance::getKey() const {
  return key;
}

facebook::jsi::Value ReactDOMInstance::get(
  facebook::jsi::Runtime& rt,
  const facebook::jsi::PropNameID& name) {
  auto nameStr = name.utf8(rt);
  if (nameStr == "tagName") {
    return facebook::jsi::String::createFromUtf8(rt, tagName);
  }
  if (nameStr == "className") {
    return facebook::jsi::String::createFromUtf8(rt, className);
  }
  if (nameStr == "key") {
    return facebook::jsi::String::createFromUtf8(rt, key);
  }
  if (nameStr == "textContent") {
    return facebook::jsi::String::createFromUtf8(rt, getTextContent());
  }
  if (nameStr == "children") {
    facebook::jsi::Array array(rt, children.size());
    for (size_t i = 0; i < children.size(); ++i) {
      array.setValueAtIndex(rt, i, facebook::jsi::Object::createFromHostObject(rt, children[i]));
    }
    return array;
  }

  facebook::jsi::Value attribute = getAttribute(rt, nameStr);
  if (!attribute.isUndefined()) {
    return attribute;
  }
  return facebook::jsi::Value::undefined();
}

void ReactDOMInstance::set(
  facebook::jsi::Runtime& rt,
  const facebook::jsi::PropNameID& name,
  const facebook::jsi::Value& value) {
  auto nameStr = name.utf8(rt);
  if (nameStr == "className" && value.isString()) {
    className = value.asString(rt).utf8(rt);
    setAttribute("class", value);
    return;
  }

  if (nameStr == "key" && value.isString()) {
    setKey(value.asString(rt).utf8(rt));
    return;
  }

  if (nameStr == "textContent") {
    if (value.isString()) {
      setTextContent(value.asString(rt).utf8(rt));
    } else if (value.isNumber()) {
      setTextContent(std::to_string(value.getNumber()));
    } else if (value.isNull() || value.isUndefined()) {
      setTextContent("");
    }
    return;
  }

  setAttribute(nameStr, value);
}

std::vector<facebook::jsi::PropNameID> ReactDOMInstance::getPropertyNames(facebook::jsi::Runtime& rt) {
  std::vector<facebook::jsi::PropNameID> props;
  props.push_back(facebook::jsi::PropNameID::forUtf8(rt, "tagName"));
  props.push_back(facebook::jsi::PropNameID::forUtf8(rt, "className"));
  props.push_back(facebook::jsi::PropNameID::forUtf8(rt, "key"));
  props.push_back(facebook::jsi::PropNameID::forUtf8(rt, "children"));
  props.push_back(facebook::jsi::PropNameID::forUtf8(rt, "textContent"));
  return props;
}

} // namespace react
