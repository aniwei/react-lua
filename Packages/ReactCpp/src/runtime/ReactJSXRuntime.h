#pragma once

#include "runtime/ReactWasmLayout.h"
#include "jsi/jsi.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace react::jsx {

namespace jsi = facebook::jsi;

struct ReactElement;

using ReactElementPtr = std::shared_ptr<ReactElement>;

struct SourceLocation {
  std::string fileName;
  int lineNumber{0};
  int columnNumber{0};

  [[nodiscard]] bool isValid() const {
    return !fileName.empty();
  }
};

enum class ValueKind : uint8_t {
  Null,
  Undefined,
  Boolean,
  Number,
  String,
  Element,
  Array,
};

struct Value {
  ValueKind kind{ValueKind::Undefined};
  std::variant<std::monostate, bool, double, std::string, ReactElementPtr, std::vector<Value>> payload{};

  Value() = default;

  static Value null();
  static Value undefined();
  static Value boolean(bool value);
  static Value number(double value);
  static Value string(std::string value);
  static Value element(const ReactElementPtr& value);
  static Value array(std::vector<Value> values);

  [[nodiscard]] bool isTruthyRenderable() const;
};

using PropEntry = std::pair<std::string, jsi::Value>;
using PropList = std::vector<PropEntry>;

struct ReactElement {
  jsi::Value type;
  std::optional<jsi::Value> key;
  std::optional<jsi::Value> ref;
  PropList props;
  std::vector<Value> children;
  std::optional<SourceLocation> source;
  
  std::optional<std::string> debugType;
  std::optional<std::string> debugKey;
};

ReactElementPtr jsx(
    jsi::Runtime& runtime,
    const jsi::Value& type,
    const jsi::Value& props,
    std::optional<jsi::Value> key = std::nullopt,
    std::optional<jsi::Value> ref = std::nullopt);

ReactElementPtr jsxs(
    jsi::Runtime& runtime,
    const jsi::Value& type,
    const jsi::Value& props,
    std::optional<jsi::Value> key = std::nullopt,
    std::optional<jsi::Value> ref = std::nullopt);

ReactElementPtr jsxDEV(
    jsi::Runtime& runtime,
    const jsi::Value& type,
    PropList config = {},
    std::optional<jsi::Value> maybeKey = std::nullopt,
    SourceLocation source = {},
    std::optional<jsi::Value> ref = std::nullopt);

struct WasmSerializedLayout {
  std::vector<uint8_t> buffer;
  uint32_t rootOffset{0};
};

WasmSerializedLayout serializeToWasm(jsi::Runtime& runtime, const ReactElement& element);

jsi::Value createJsxHostValue(jsi::Runtime& runtime, const ReactElementPtr& element);
ReactElementPtr getReactElementFromValue(jsi::Runtime& runtime, const jsi::Value& value);
bool isReactElementValue(jsi::Runtime& runtime, const jsi::Value& value);

} // namespace react::jsx
