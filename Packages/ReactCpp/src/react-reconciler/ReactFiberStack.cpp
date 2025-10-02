#include "react-reconciler/ReactFiberStack.h"

namespace react {
namespace detail {

std::vector<std::any>& valueStack() {
  static std::vector<std::any> stack;
  return stack;
}

std::vector<const FiberNode*>& fiberStack() {
  static std::vector<const FiberNode*> stack;
  return stack;
}

int& stackIndex() {
  static int index = -1;
  return index;
}

} // namespace detail
} // namespace react
