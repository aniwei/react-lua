#include "react-reconciler/ReactWakeable.h"

#include <mutex>
#include <unordered_set>

namespace react {
namespace {

std::unordered_set<const Wakeable*>& registry() {
  static std::unordered_set<const Wakeable*> set;
  return set;
}

std::mutex& registryMutex() {
  static std::mutex mutex;
  return mutex;
}

} // namespace

Wakeable::Wakeable() {
  std::lock_guard<std::mutex> lock{registryMutex()};
  registry().insert(this);
}

Wakeable::~Wakeable() {
  std::lock_guard<std::mutex> lock{registryMutex()};
  registry().erase(this);
}

bool isWakeableValue(const void* value) {
  if (value == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock{registryMutex()};
  return registry().count(static_cast<const Wakeable*>(value)) > 0;
}

Wakeable* tryGetWakeable(void* value) {
  return isWakeableValue(value) ? static_cast<Wakeable*>(value) : nullptr;
}

const Wakeable* tryGetWakeable(const void* value) {
  return isWakeableValue(value) ? static_cast<const Wakeable*>(value) : nullptr;
}

} // namespace react
