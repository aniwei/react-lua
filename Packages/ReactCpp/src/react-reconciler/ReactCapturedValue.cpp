#include "react-reconciler/ReactCapturedValue.h"

#include "react-reconciler/ReactFiber.h"

#include <unordered_map>

namespace react {

namespace {

std::unordered_map<void*, CapturedValue>& capturedStacks() {
  static std::unordered_map<void*, CapturedValue> storage;
  return storage;
}

std::string getStackByFiberInDevAndProd(const FiberNode*) {
  // TODO: Integrate with ReactFiberComponentStack when available.
  return std::string{};
}

} // namespace

CapturedValue createCapturedValueAtFiber(void* value, FiberNode* source) {
  if (value != nullptr) {
    auto& storage = capturedStacks();
    auto it = storage.find(value);
    if (it != storage.end()) {
      return it->second;
    }

    CapturedValue captured{value, source, getStackByFiberInDevAndProd(source)};
    storage.emplace(value, captured);
    return captured;
  }

  return CapturedValue{value, source, getStackByFiberInDevAndProd(source)};
}

CapturedValue createCapturedValueFromError(void* value, std::string stack) {
  CapturedValue captured{value, nullptr, std::move(stack)};
  if (!captured.stack.empty() && value != nullptr) {
    capturedStacks().insert_or_assign(value, captured);
  }
  return captured;
}

} // namespace react
