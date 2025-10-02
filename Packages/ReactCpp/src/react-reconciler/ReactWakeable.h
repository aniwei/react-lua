#pragma once

#include <functional>

namespace react {

class Wakeable {
 public:
  Wakeable();
  virtual ~Wakeable();

  Wakeable(const Wakeable&) = delete;
  Wakeable& operator=(const Wakeable&) = delete;

  Wakeable(Wakeable&&) = delete;
  Wakeable& operator=(Wakeable&&) = delete;

  virtual void then(
    std::function<void()> onFulfilled,
    std::function<void()> onRejected) = 0;
};

bool isWakeableValue(const void* value);
Wakeable* tryGetWakeable(void* value);
const Wakeable* tryGetWakeable(const void* value);

} // namespace react
