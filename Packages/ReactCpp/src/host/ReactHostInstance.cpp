#include "ReactHostInstance.h"

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

ReactHostInstance::ReactHostInstance(jsi::Runtime& rt, std::string type, const jsi::Object& props)
  : runtime_(&rt),
    isText_(false),
    type_(std::move(type)),
    props_(cloneProps(rt, props)) {}

ReactHostInstance::ReactHostInstance(jsi::Runtime& rt, std::string type, const std::string& textContent)
  : runtime_(&rt),
    isText_(true),
    type_(std::move(type)),
    textContent_(textContent) {}

void ReactHostInstance::appendChild(std::shared_ptr<HostInstance> child) {
  if (!child || isText_) {
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

void ReactHostInstance::removeChild(std::shared_ptr<HostInstance> child) {
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

void ReactHostInstance::insertChildBefore(
  std::shared_ptr<HostInstance> child,
  std::shared_ptr<HostInstance> beforeChild) {
  if (!child || isText_) {
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

void ReactHostInstance::setAttribute(
  const std::string& key,
  const jsi::Value& value) {
  if (!runtime_) {
    return;
  }

  if (value.isUndefined()) {
    removeAttribute(key);
    return;
  }

  if (key == "class" || key == "className") {
    if (value.isString()) {
      className = value.asString(*runtime_).utf8(*runtime_);
    } else if (value.isNull()) {
      className.clear();
    }
  }

  if (key == "textContent") {
    if (value.isString()) {
      setTextContent(value.asString(*runtime_).utf8(*runtime_));
    } else if (value.isNumber()) {
      setTextContent(std::to_string(value.getNumber()));
    } else if (value.isNull()) {
      setTextContent("");
    }
    return;
  }

  if (isEventPropKey(key)) {
    if (value.isNull()) {
      eventHandlers_.erase(key);
      props_.erase(key);
      return;
    }

    if (value.isObject()) {
      eventHandlers_[key] = jsi::Value(*runtime_, value);
    } else {
      eventHandlers_.erase(key);
    }
  }

  setProp(key, value);
}

void ReactHostInstance::removeAttribute(const std::string& key) {
  if (key == "class" || key == "className") {
    className.clear();
  }
  if (key == "textContent") {
    textContent_.clear();
    props_.erase("textContent");
    return;
  }
  if (isEventPropKey(key)) {
    eventHandlers_.erase(key);
  }
  removeProp(key);
}

jsi::Value ReactHostInstance::getAttribute(
  jsi::Runtime& rt,
  const std::string& key) const {
  auto eventIt = eventHandlers_.find(key);
  if (eventIt != eventHandlers_.end()) {
    return jsi::Value(rt, eventIt->second);
  }

  auto it = props_.find(key);
  if (it == props_.end()) {
    if (key == "class" || key == "className") {
      return jsi::Value(jsi::String::createFromUtf8(rt, className));
    }
    if (key == "textContent") {
      return jsi::Value(jsi::String::createFromUtf8(rt, textContent_));
    }
    return jsi::Value::undefined();
  }

  return jsi::Value(rt, it->second);
}

void ReactHostInstance::setTextContent(const std::string& text) {
  textContent_ = text;
  if (!runtime_) {
    return;
  }

  if (!isText_) {
    props_["textContent"] = jsi::Value(jsi::String::createFromUtf8(*runtime_, text));
  }
}

const std::string& ReactHostInstance::getTextContent() const {
  return textContent_;
}

void ReactHostInstance::setProp(
  const std::string& key,
  const jsi::Value& value) {
  if (!runtime_) {
    return;
  }

  props_[key] = jsi::Value(*runtime_, value);
}

void ReactHostInstance::removeProp(const std::string& key) {
  props_.erase(key);
}

void ReactHostInstance::applyUpdate(
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
      } else if (textValue.isNull() || textValue.isUndefined()) {
        textContent_.clear();
      }
    }
    return;
  }

  props_ = cloneProps(rt, newProps);

  auto applySetFromObject = [&](const char* propertyName) {
    jsi::PropNameID nameID = jsi::PropNameID::forUtf8(rt, propertyName);
    if (!payload.hasProperty(rt, nameID)) {
      return;
    }

    jsi::Value container = payload.getProperty(rt, nameID);
    if (!container.isObject()) {
      return;
    }

    jsi::Object object = container.asObject(rt);
    jsi::Array names = object.getPropertyNames(rt);
    size_t length = names.size(rt);
    for (size_t i = 0; i < length; ++i) {
      jsi::Value nameValue = names.getValueAtIndex(rt, i);
      if (!nameValue.isString()) {
        continue;
      }
      std::string key = nameValue.asString(rt).utf8(rt);
      jsi::PropNameID entryID = jsi::PropNameID::forUtf8(rt, key);
      jsi::Value entryValue = object.getProperty(rt, entryID);
      setAttribute(key, entryValue);
    }
  };

  auto applyRemovalFromArray = [&](const char* propertyName) {
    jsi::PropNameID nameID = jsi::PropNameID::forUtf8(rt, propertyName);
    if (!payload.hasProperty(rt, nameID)) {
      return;
    }

    jsi::Value container = payload.getProperty(rt, nameID);
    if (!container.isObject()) {
      return;
    }

    jsi::Object object = container.asObject(rt);
    if (!object.isArray(rt)) {
      return;
    }

    jsi::Array array = object.asArray(rt);
    size_t length = array.size(rt);
    for (size_t i = 0; i < length; ++i) {
      jsi::Value value = array.getValueAtIndex(rt, i);
      if (!value.isString()) {
        continue;
      }
      removeAttribute(value.asString(rt).utf8(rt));
    }
  };

  applySetFromObject("attributes");
  applySetFromObject("events");
  applyRemovalFromArray("removedAttributes");
  applyRemovalFromArray("removedEvents");

  jsi::PropNameID textProp = jsi::PropNameID::forUtf8(rt, "text");
  if (payload.hasProperty(rt, textProp)) {
    jsi::Value textValue = payload.getProperty(rt, textProp);
    if (textValue.isString()) {
      setTextContent(textValue.asString(rt).utf8(rt));
    } else if (textValue.isNumber()) {
      setTextContent(std::to_string(textValue.getNumber()));
    } else if (textValue.isNull() || textValue.isUndefined()) {
      setTextContent("");
    }
  }
}

} // namespace react
