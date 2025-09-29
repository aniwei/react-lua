#include "ReactDOMDiffProperties.h"

#include <algorithm>

namespace jsi = facebook::jsi;

namespace react {

namespace {

std::string coerceTextValue(jsi::Runtime& rt, const jsi::Value& value) {
  if (value.isString()) {
    return value.asString(rt).utf8(rt);
  }

  if (value.isNumber()) {
    double number = value.getNumber();
    if (number == 0) {
      return "0";
    }
    std::string asText = std::to_string(number);
    if (asText.find('.') != std::string::npos) {
      while (!asText.empty() && asText.back() == '0') {
        asText.pop_back();
      }
      if (!asText.empty() && asText.back() == '.') {
        asText.pop_back();
      }
      if (asText.empty()) {
        asText = "0";
      }
    }
    return asText;
  }

  if (value.isNull() || value.isUndefined()) {
    return std::string();
  }

  if (value.isObject()) {
    jsi::Object object = value.asObject(rt);
    if (object.hasProperty(rt, "toString")) {
      jsi::Value toStringValue = object.getProperty(rt, "toString");
      if (toStringValue.isString()) {
        return toStringValue.asString(rt).utf8(rt);
      }
    }
  }

  return std::string();
}

void collectKeys(jsi::Runtime& rt, const jsi::Value& value, std::vector<std::string>& out) {
  if (!value.isObject()) {
    return;
  }

  jsi::Object object = value.asObject(rt);
  jsi::Array names = object.getPropertyNames(rt);
  size_t length = names.size(rt);
  out.reserve(out.size() + length);

  for (size_t i = 0; i < length; ++i) {
    jsi::Value keyValue = names.getValueAtIndex(rt, i);
    if (!keyValue.isString()) {
      continue;
    }
    std::string key = keyValue.asString(rt).utf8(rt);
    if (key == "children") {
      continue;
    }
    if (std::find(out.begin(), out.end(), key) == out.end()) {
      out.push_back(std::move(key));
    }
  }
}

jsi::Value cloneValue(jsi::Runtime& rt, const jsi::Value& value) {
  if (value.isUndefined()) {
    return jsi::Value::undefined();
  }
  if (value.isNull()) {
    return jsi::Value::null();
  }
  if (value.isBool()) {
    return jsi::Value(value.getBool());
  }
  if (value.isNumber()) {
    return jsi::Value(value.getNumber());
  }
  return jsi::Value(rt, value);
}

} // namespace

bool ReactDOMDiffPropertiesResult::hasChanges() const {
  return !attributesToSet.empty() ||
    !attributesToRemove.empty() ||
    !eventsToSet.empty() ||
    !eventsToRemove.empty() ||
    textContent.has_value();
}

bool isReactDOMEventPropKey(const std::string& key) {
  if (key.size() <= 2) {
    return false;
  }

  const char first = key[0];
  const char second = key[1];
  if (!((first == 'o' || first == 'O') && (second == 'n' || second == 'N'))) {
    return false;
  }

  unsigned char third = static_cast<unsigned char>(key[2]);
  return third >= 'A' && third <= 'Z';
}

ReactDOMDiffPropertiesResult diffReactDOMProperties(
  jsi::Runtime& rt,
  const jsi::Value& previousProps,
  const jsi::Value& nextProps) {
  ReactDOMDiffPropertiesResult result;

  std::vector<std::string> keys;
  collectKeys(rt, previousProps, keys);
  collectKeys(rt, nextProps, keys);

  auto pushUnique = [](std::vector<std::string>& list, const std::string& value) {
    if (std::find(list.begin(), list.end(), value) == list.end()) {
      list.push_back(value);
    }
  };

  auto handleRemoval = [&](const std::string& key, bool isEvent) {
    if (isEvent) {
      pushUnique(result.eventsToRemove, key);
    } else if (key == "className" || key == "class") {
      result.attributesToSet.erase(key);
      pushUnique(result.attributesToRemove, key);
    } else if (key == "textContent") {
      result.textContent = std::string();
    } else {
      pushUnique(result.attributesToRemove, key);
    }
  };

  jsi::Object previousObject = previousProps.isObject() ? previousProps.asObject(rt) : jsi::Object(rt);
  jsi::Object nextObject = nextProps.isObject() ? nextProps.asObject(rt) : jsi::Object(rt);

  for (const std::string& key : keys) {
    bool isEvent = isReactDOMEventPropKey(key);
    jsi::PropNameID nameID = jsi::PropNameID::forUtf8(rt, key);

    bool nextHas = nextProps.isObject() && nextObject.hasProperty(rt, nameID);
    bool previousHas = previousProps.isObject() && previousObject.hasProperty(rt, nameID);

    jsi::Value previousValue = previousHas ? previousObject.getProperty(rt, nameID) : jsi::Value::undefined();
    jsi::Value nextValue = nextHas ? nextObject.getProperty(rt, nameID) : jsi::Value::undefined();

    if (!nextHas) {
      if (previousHas) {
        handleRemoval(key, isEvent);
      }
      continue;
    }

    if (isEvent) {
      if (nextValue.isNull() || nextValue.isUndefined()) {
        handleRemoval(key, true);
        continue;
      }

      if (!previousHas || !jsi::Value::strictEquals(rt, previousValue, nextValue)) {
        result.eventsToSet.emplace(key, cloneValue(rt, nextValue));
      }
      continue;
    }

    if (key == "textContent") {
      std::string text = coerceTextValue(rt, nextValue);
      std::string previousText = previousHas ? coerceTextValue(rt, previousValue) : std::string();
      if (!previousHas || text != previousText) {
        result.textContent = std::move(text);
      }
      continue;
    }

    if (!nextValue.isUndefined() && !(nextValue.isNull() && !previousHas)) {
      if (!previousHas || !jsi::Value::strictEquals(rt, previousValue, nextValue)) {
        result.attributesToSet.emplace(key, cloneValue(rt, nextValue));
      }
    } else {
      handleRemoval(key, false);
    }
  }

  return result;
}

} // namespace react
