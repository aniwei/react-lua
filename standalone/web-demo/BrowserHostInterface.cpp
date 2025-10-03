#ifdef __EMSCRIPTEN__

#include "BrowserHostInterface.h"

#include "runtime/ReactHostInterface.h"

#include "jsi/jsi.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace {

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

} // namespace

namespace react::browser {

BrowserHostInterface::BrowserHostInterface() = default;
BrowserHostInterface::~BrowserHostInterface() = default;

void BrowserHostInterface::attachRuntime(facebook::jsi::Runtime& runtime) {
  if (runtime_ == &runtime) {
    return;
  }
  runtime_ = &runtime;
}

void BrowserHostInterface::ensureRuntimeAttached() {
  if (!runtime_) {
    throw std::runtime_error("BrowserHostInterface runtime not attached");
  }
}

emscripten::val BrowserHostInterface::document() {
  return emscripten::val::global("document");
}

std::shared_ptr<ReactDOMInstance> BrowserHostInterface::createHostInstance(
    facebook::jsi::Runtime& runtime,
    const std::string& type,
    const facebook::jsi::Object& props) {
  attachRuntime(runtime);
  emscripten::val element = document().call<emscripten::val>("createElement", emscripten::val(type));
  auto instance = HostInterface::createHostInstance(runtime, type, props);
  trackInstance(instance, element);
  applyInitialProps(props, element);
  return instance;
}

std::shared_ptr<ReactDOMInstance> BrowserHostInterface::createHostTextInstance(
    facebook::jsi::Runtime& runtime,
    const std::string& text) {
  attachRuntime(runtime);
  emscripten::val node = document().call<emscripten::val>("createTextNode", emscripten::val(text));
  auto instance = HostInterface::createHostTextInstance(runtime, text);
  trackInstance(instance, node);
  return instance;
}

void BrowserHostInterface::appendHostChild(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child) {
  HostInterface::appendHostChild(parent, child);
  auto parentNode = getNode(parent);
  auto childNode = getNode(child);
  if (!parentNode.isUndefined() && !childNode.isUndefined()) {
    parentNode.call<void>("appendChild", childNode);
  }
}

void BrowserHostInterface::removeHostChild(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child) {
  auto parentNode = getNode(parent);
  auto childNode = getNode(child);
  if (!parentNode.isUndefined() && !childNode.isUndefined()) {
    parentNode.call<void>("removeChild", childNode);
  }
  HostInterface::removeHostChild(parent, child);
  untrackInstance(child.get());
}

void BrowserHostInterface::insertHostChildBefore(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child,
    std::shared_ptr<ReactDOMInstance> beforeChild) {
  HostInterface::insertHostChildBefore(parent, child, beforeChild);
  auto parentNode = getNode(parent);
  auto childNode = getNode(child);
  if (parentNode.isUndefined() || childNode.isUndefined()) {
    return;
  }
  if (beforeChild) {
    auto beforeNode = getNode(beforeChild);
    if (!beforeNode.isUndefined()) {
      parentNode.call<void>("insertBefore", childNode, beforeNode);
      return;
    }
  }
  parentNode.call<void>("appendChild", childNode);
}

void BrowserHostInterface::commitHostUpdate(
    std::shared_ptr<ReactDOMInstance> instance,
    const facebook::jsi::Object& oldProps,
    const facebook::jsi::Object& newProps,
    const facebook::jsi::Object& payload) {
  (void)oldProps;
  HostInterface::commitHostUpdate(instance, oldProps, newProps, payload);
  auto element = getNode(instance);
  if (element.isUndefined()) {
    return;
  }
  applyPayload(payload, element);
}

void BrowserHostInterface::commitHostTextUpdate(
    std::shared_ptr<ReactDOMInstance> instance,
    const std::string& oldText,
    const std::string& newText) {
  HostInterface::commitHostTextUpdate(instance, oldText, newText);
  auto node = getNode(instance);
  if (node.isUndefined()) {
    return;
  }
  node.set("nodeValue", emscripten::val(newText));
}

std::shared_ptr<ReactDOMInstance> BrowserHostInterface::createRootContainer(const std::string& elementId) {
  ensureRuntimeAttached();
  emscripten::val doc = document();
  emscripten::val root = doc.call<emscripten::val>("getElementById", emscripten::val(elementId));
  if (root.isUndefined() || root.isNull()) {
    root = doc.call<emscripten::val>("createElement", emscripten::val("div"));
    root.set("id", emscripten::val(elementId));
    doc["body"].call<void>("appendChild", root);
  }

  facebook::jsi::Object props(*runtime_);
  auto container = HostInterface::createHostInstance(*runtime_, "div", props);
  trackInstance(container, root);
  return container;
}

emscripten::val BrowserHostInterface::getNode(const std::shared_ptr<ReactDOMInstance>& instance) const {
  if (!instance) {
    return emscripten::val::undefined();
  }
  return getNode(instance.get());
}

emscripten::val BrowserHostInterface::getNode(ReactDOMInstance* instance) const {
  if (!instance) {
    return emscripten::val::undefined();
  }
  auto it = nodes_.find(instance);
  if (it == nodes_.end()) {
    return emscripten::val::undefined();
  }
  return it->second;
}

void BrowserHostInterface::trackInstance(const std::shared_ptr<ReactDOMInstance>& instance, const emscripten::val& node) {
  if (!instance) {
    return;
  }
  nodes_[instance.get()] = node;
}

void BrowserHostInterface::untrackInstance(ReactDOMInstance* instance) {
  if (!instance) {
    return;
  }
  nodes_.erase(instance);
}

void BrowserHostInterface::applyInitialProps(
    const facebook::jsi::Object& props,
    const emscripten::val& element) {
  if (!runtime_) {
    return;
  }
  facebook::jsi::Runtime& rt = *runtime_;
  facebook::jsi::Array names = props.getPropertyNames(rt);
  size_t length = names.size(rt);
  for (size_t i = 0; i < length; ++i) {
    facebook::jsi::Value nameValue = names.getValueAtIndex(rt, i);
    if (!nameValue.isString()) {
      continue;
    }
    std::string key = nameValue.asString(rt).utf8(rt);
    if (key == "children") {
      continue;
    }
    facebook::jsi::Value value = props.getProperty(rt, key.c_str());
    applyAttribute(key, value, element);
  }
}

void BrowserHostInterface::applyAttribute(
    const std::string& key,
    const facebook::jsi::Value& value,
    const emscripten::val& element) {
  if (!runtime_) {
    return;
  }
  if (key == "children") {
    return;
  }
  if (key == "class" || key == "className") {
    if (value.isString()) {
      element.set("className", emscripten::val(value.getString(*runtime_).utf8(*runtime_)));
    } else if (value.isNull() || value.isUndefined()) {
      element.set("className", emscripten::val(""));
    }
    return;
  }
  if (key == "textContent") {
    if (value.isString()) {
      element.set("textContent", emscripten::val(value.getString(*runtime_).utf8(*runtime_)));
    } else if (value.isNumber()) {
      element.set("textContent", emscripten::val(numberToString(value.getNumber())));
    } else {
      element.set("textContent", emscripten::val(""));
    }
    return;
  }
  if (value.isNull() || value.isUndefined()) {
    removeAttribute(key, element);
    return;
  }
  if (value.isBool()) {
    bool boolValue = value.getBool();
    element.set(key.c_str(), emscripten::val(boolValue));
    if (boolValue) {
      element.call<void>("setAttribute", emscripten::val(key), emscripten::val(""));
    } else {
      element.call<void>("removeAttribute", emscripten::val(key));
    }
    return;
  }
  if (value.isNumber()) {
    element.call<void>("setAttribute", emscripten::val(key), emscripten::val(numberToString(value.getNumber())));
    return;
  }
  if (value.isString()) {
    element.call<void>("setAttribute", emscripten::val(key), emscripten::val(value.getString(*runtime_).utf8(*runtime_)));
    return;
  }
}

void BrowserHostInterface::removeAttribute(
    const std::string& key,
    const emscripten::val& element) {
  if (key == "class" || key == "className") {
    element.set("className", emscripten::val(""));
    element.call<void>("removeAttribute", emscripten::val("class"));
    element.call<void>("removeAttribute", emscripten::val("className"));
    return;
  }
  if (key == "textContent") {
    element.set("textContent", emscripten::val(""));
    return;
  }
  element.call<void>("removeAttribute", emscripten::val(key));
  element.set(key.c_str(), emscripten::val::undefined());
}

void BrowserHostInterface::applyPayload(
    const facebook::jsi::Object& payload,
    const emscripten::val& element) {
  if (!runtime_) {
    return;
  }
  facebook::jsi::Runtime& rt = *runtime_;
  facebook::jsi::PropNameID attributesId = facebook::jsi::PropNameID::forUtf8(rt, "attributes");
  if (payload.hasProperty(rt, attributesId)) {
    facebook::jsi::Value attributesValue = payload.getProperty(rt, attributesId);
    if (attributesValue.isObject()) {
      facebook::jsi::Object attributes = attributesValue.asObject(rt);
      facebook::jsi::Array names = attributes.getPropertyNames(rt);
      size_t length = names.size(rt);
      for (size_t i = 0; i < length; ++i) {
        facebook::jsi::Value nameValue = names.getValueAtIndex(rt, i);
        if (!nameValue.isString()) {
          continue;
        }
        std::string key = nameValue.asString(rt).utf8(rt);
        facebook::jsi::Value propValue = attributes.getProperty(rt, key.c_str());
        applyAttribute(key, propValue, element);
      }
    }
  }

  facebook::jsi::PropNameID removedAttributesId = facebook::jsi::PropNameID::forUtf8(rt, "removedAttributes");
  if (payload.hasProperty(rt, removedAttributesId)) {
    facebook::jsi::Value removedValue = payload.getProperty(rt, removedAttributesId);
    if (removedValue.isObject()) {
      facebook::jsi::Object removed = removedValue.asObject(rt);
      if (removed.isArray(rt)) {
        facebook::jsi::Array array = removed.asArray(rt);
        size_t length = array.size(rt);
        for (size_t i = 0; i < length; ++i) {
          facebook::jsi::Value entry = array.getValueAtIndex(rt, i);
          if (entry.isString()) {
            removeAttribute(entry.getString(rt).utf8(rt), element);
          }
        }
      }
    }
  }

  facebook::jsi::PropNameID textId = facebook::jsi::PropNameID::forUtf8(rt, "text");
  if (payload.hasProperty(rt, textId)) {
    facebook::jsi::Value textValue = payload.getProperty(rt, textId);
    if (textValue.isString()) {
      element.set("textContent", emscripten::val(textValue.getString(rt).utf8(rt)));
    } else if (textValue.isNumber()) {
      element.set("textContent", emscripten::val(numberToString(textValue.getNumber())));
    } else {
      element.set("textContent", emscripten::val(""));
    }
  }
}

} // namespace react::browser

#endif // __EMSCRIPTEN__
