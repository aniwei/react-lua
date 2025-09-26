#pragma once

#include <functional>

namespace react {

using Task = std::function<void()>;

// Scheduler interface defines how tasks are scheduled (e.g., requestIdleCallback, setTimeout)
class Scheduler {
public:
  virtual ~Scheduler() = default;

  // Schedules a high-priority task (e.g., user input).
  virtual void scheduleImmediateTask(Task task) = 0;

  // Schedules a normal-priority task (like requestIdleCallback).
  virtual void scheduleNormalTask(Task task) = 0;
};

} // namespace react
