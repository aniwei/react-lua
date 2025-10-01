#pragma once

#include <string>

namespace react {

class FiberNode;

struct CapturedValue {
  void* value{nullptr};
  FiberNode* source{nullptr};
  std::string stack{};
};

CapturedValue createCapturedValueAtFiber(void* value, FiberNode* source);
CapturedValue createCapturedValueFromError(void* value, std::string stack);

} // namespace react
