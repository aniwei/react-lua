#include "SimpleHostInstance.h"

#include <algorithm>

namespace jsi = facebook::jsi;

namespace react {

namespace {
std::unordered_map<std::string, jsi::Value> cloneProps(
  jsi::Runtime& rt,
  const jsi::Object& props) {
  std::unordered_map<std::string, jsi::Value> result;
  jsi::Array propertyNames = props.getPropertyNames(rt);
  size_t length = propertyNames.size(rt);
  result.reserve(length);

  for (size_t i = 0; i < length; ++i) {
    jsi::Value nameValue = propertyNames.getValueAtIndex(rt, i);
    if (!nameValue.isString()) {
      continue;
    }

    std::string key = nameValue.asString(rt).utf8(rt);
    jsi::PropNameID nameID = jsi::PropNameID::forUtf8(rt, key);
    jsi::Value value = props.getProperty(rt, nameID);

    result.emplace(std::move(key), jsi::Value(rt, value));
  }

  return result;
}
} // namespace

SimpleHostInstance::SimpleHostInstance(jsi::Runtime& rt, std::string type, const jsi::Object& props)
  : runtime_(&rt),
    isText_(false),
    type_(std::move(type)),
    props_(cloneProps(rt, props)) {}

SimpleHostInstance::SimpleHostInstance(jsi::Runtime& rt, std::string type, const std::string& textContent)
  : runtime_(&rt),
    isText_(true),
    type_(std::move(type)),
    textContent_(textContent) {}

void SimpleHostInstance::appendChild(std::shared_ptr<HostInstance> child) {
  if (!child) {
    return;
  }

  if (isText_) {
    return;
  }

  auto existing = std::find_if(children.begin(), children.end(), [&](const std::shared_ptr<HostInstance>& current) {
    return current.get() == child.get();
  });

  if (existing != children.end()) {
    return;
  }

  children.push_back(child);
  child->parent = shared_from_this();
}

void SimpleHostInstance::removeChild(std::shared_ptr<HostInstance> child) {
  if (!child) {
    return;
  }

  auto it = std::find_if(children.begin(), children.end(), [&](const std::shared_ptr<HostInstance>& current) {
    return current.get() == child.get();
  });

  if (it == children.end()) {
    return;
  }

  (*it)->parent.reset();
  children.erase(it);
}

void SimpleHostInstance::insertChildBefore(
  std::shared_ptr<HostInstance> child,
  std::shared_ptr<HostInstance> beforeChild) {
  if (!child) {
    return;
  }

  if (isText_) {
    return;
  }

  auto removeIt = std::find_if(children.begin(), children.end(), [&](const std::shared_ptr<HostInstance>& current) {
    return current.get() == child.get();
  });
  if (removeIt != children.end()) {
    children.erase(removeIt);
  }

  if (!beforeChild) {
    appendChild(child);
    return;
  }

  auto beforeIt = std::find_if(children.begin(), children.end(), [&](const std::shared_ptr<HostInstance>& current) {
    return current.get() == beforeChild.get();
  });

  if (beforeIt == children.end()) {
    appendChild(child);
    return;
  }

  child->parent = shared_from_this();
  children.insert(beforeIt, child);
}

void SimpleHostInstance::applyUpdate(
  const jsi::Object& newProps,
  const jsi::Object& payload) {
  if (!runtime_) {
    return;
  }

  jsi::Runtime& rt = *runtime_;

  if (isText_) {
    if (payload.hasProperty(rt, "text")) {
      jsi::Value textValue = payload.getProperty(rt, "text");
      if (textValue.isString()) {
        textContent_ = textValue.asString(rt).utf8(rt);
      } else if (textValue.isNumber()) {
        textContent_ = std::to_string(textValue.getNumber());
      }
    }
    return;
  }

  props_ = cloneProps(rt, newProps);

  jsi::Array names = payload.getPropertyNames(rt);
  size_t length = names.size(rt);

  for (size_t i = 0; i < length; ++i) {
    jsi::Value nameValue = names.getValueAtIndex(rt, i);
    if (!nameValue.isString()) {
      continue;
    }

    std::string key = nameValue.asString(rt).utf8(rt);
    jsi::PropNameID nameID = jsi::PropNameID::forUtf8(rt, key);
    jsi::Value value = payload.getProperty(rt, nameID);

    if (value.isUndefined()) {
      removeProp(key);
      continue;
    }

    setProp(key, value);
  }
}

void SimpleHostInstance::setProp(const std::string& key, const jsi::Value& value) {
  if (!runtime_) {
    return;
  }

  props_[key] = jsi::Value(*runtime_, value);
}

void SimpleHostInstance::removeProp(const std::string& key) {
  props_.erase(key);
}

} // namespace react
