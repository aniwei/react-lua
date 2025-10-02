#pragma once

#include <any>
#include <utility>
#include <vector>

namespace react {

class FiberNode;

namespace detail {
std::vector<std::any>& valueStack();
std::vector<const FiberNode*>& fiberStack();
int& stackIndex();
} // namespace detail

template <typename T>
struct StackCursor {
  T current;
};

template <typename T>
inline StackCursor<T> createCursor(T defaultValue) {
  return StackCursor<T>{std::move(defaultValue)};
}

template <typename T>
inline void push(StackCursor<T>& cursor, T value, FiberNode* fiber) {
  auto& index = detail::stackIndex();
  auto& valueStack = detail::valueStack();
  auto& fiberStack = detail::fiberStack();

  ++index;
  if (index >= static_cast<int>(valueStack.size())) {
    valueStack.emplace_back();
    fiberStack.emplace_back(nullptr);
  }

  valueStack[index] = cursor.current;
  fiberStack[index] = fiber;
  cursor.current = std::move(value);
}

template <typename T>
inline void pop(StackCursor<T>& cursor, FiberNode* fiber) {
  auto& index = detail::stackIndex();
  auto& valueStack = detail::valueStack();
  auto& fiberStack = detail::fiberStack();

  if (index < 0) {
    return;
  }

#ifndef NDEBUG
  if (fiberStack[index] != nullptr && fiberStack[index] != fiber) {
    // Unexpected fiber popped; this mirrors the development warning in React.
    // We intentionally ignore the mismatch in release builds.
  }
#endif

  if (auto* stored = std::any_cast<T>(&valueStack[index])) {
    cursor.current = std::move(*stored);
  } else {
    cursor.current = T{};
  }

  valueStack[index].reset();
  fiberStack[index] = nullptr;
  --index;
}

} // namespace react
