#include "react-reconciler/ReactFiberRootScheduler.h"

#include "react-reconciler/ReactFiber.h"
#include "react-reconciler/ReactFiberLane.h"
#include "react-reconciler/ReactFiberWorkLoop.h"
#include "runtime/ReactRuntime.h"

namespace react {
namespace {

FiberRoot* firstScheduledRoot = nullptr;
FiberRoot* lastScheduledRoot = nullptr;
bool didScheduleRootProcessing = false;
bool isProcessingRootSchedule = false;

void ensureScheduleProcessing(ReactRuntime& runtime);

void addRootToSchedule(FiberRoot& root) {
  if (&root == lastScheduledRoot || root.next != nullptr || firstScheduledRoot == &root) {
    return;
  }

  root.next = nullptr;
  if (lastScheduledRoot == nullptr) {
    firstScheduledRoot = lastScheduledRoot = &root;
  } else {
    lastScheduledRoot->next = &root;
    lastScheduledRoot = &root;
  }
}

void removeRootFromSchedule(FiberRoot& root) {
  FiberRoot* previous = nullptr;
  FiberRoot* current = firstScheduledRoot;
  while (current != nullptr) {
    if (current == &root) {
      if (previous == nullptr) {
        firstScheduledRoot = current->next;
      } else {
        previous->next = current->next;
      }
      if (lastScheduledRoot == &root) {
        lastScheduledRoot = previous;
      }
      root.next = nullptr;
      break;
    }
    previous = current;
    current = current->next;
  }
}

SchedulerPriority toSchedulerPriority(Lane lane) {
  switch (lanePriorityForLane(lane)) {
    case LanePriority::SyncLanePriority:
      return SchedulerPriority::ImmediatePriority;
    case LanePriority::InputContinuousLanePriority:
      return SchedulerPriority::UserBlockingPriority;
    case LanePriority::DefaultLanePriority:
    case LanePriority::TransitionLanePriority:
    case LanePriority::RetryLanePriority:
      return SchedulerPriority::NormalPriority;
    case LanePriority::IdleLanePriority:
      return SchedulerPriority::IdlePriority;
    case LanePriority::OffscreenLanePriority:
      return SchedulerPriority::LowPriority;
    case LanePriority::NoLanePriority:
    default:
      return SchedulerPriority::NormalPriority;
  }
}

void commitRoot(FiberRoot& root, FiberNode& finishedWork) {
  FiberNode* const previousCurrent = root.current;

  if (previousCurrent == &finishedWork) {
    return;
  }

  root.current = &finishedWork;

  finishedWork.alternate = previousCurrent;
  if (previousCurrent != nullptr) {
    previousCurrent->alternate = &finishedWork;
  }
}

void performScheduledWorkOnRoot(ReactRuntime& runtime, FiberRoot& root, Lane scheduledLane) {
  (void)scheduledLane;

  const Lanes lanes = getHighestPriorityPendingLanes(root);
  if (lanes == NoLanes) {
    root.callbackNode = {};
    root.callbackPriority = NoLane;
    removeRootFromSchedule(root);
    return;
  }

  const Lanes previousPendingLanes = root.pendingLanes;

  RootExitStatus status;
  if (includesBlockingLane(lanes) || includesSyncLane(lanes)) {
    status = renderRootSync(runtime, root, lanes, false);
  } else {
    status = renderRootConcurrent(runtime, root, lanes);
  }

  switch (status) {
    case RootExitStatus::Completed: {
      FiberNode* const finishedWork = root.current != nullptr ? root.current->alternate : nullptr;
      const Lanes remainingLanes = subtractLanes(previousPendingLanes, lanes);
      markRootFinished(root, lanes, remainingLanes, NoLane, NoLanes, NoLanes);

      if (finishedWork != nullptr) {
        commitRoot(root, *finishedWork);
      }
      break;
    }
    case RootExitStatus::Suspended:
    case RootExitStatus::SuspendedWithDelay:
    case RootExitStatus::SuspendedAtTheShell: {
      constexpr bool didAttemptEntireTree = false;
      markRootSuspended(root, lanes, NoLane, didAttemptEntireTree);
      break;
    }
    case RootExitStatus::Errored:
    case RootExitStatus::FatalErrored: {
      const Lanes remainingLanes = subtractLanes(previousPendingLanes, lanes);
      markRootFinished(root, lanes, remainingLanes, NoLane, NoLanes, NoLanes);
      break;
    }
    case RootExitStatus::InProgress:
      break;
  }

  root.callbackNode = {};
  root.callbackPriority = NoLane;

  if (getHighestPriorityPendingLanes(root) == NoLanes) {
    removeRootFromSchedule(root);
  } else {
    ensureScheduleProcessing(runtime);
  }
}

void scheduleRootTask(ReactRuntime& runtime, FiberRoot& root, Lane lane) {
  const SchedulerPriority priority = toSchedulerPriority(lane);
  const TaskHandle handle = runtime.scheduleTask(priority, [&runtime, rootPtr = &root, lane]() {
    performScheduledWorkOnRoot(runtime, *rootPtr, lane);
  });

  root.callbackNode = handle;
  root.callbackPriority = lane;
}

void processRootSchedule(ReactRuntime& runtime) {
  if (isProcessingRootSchedule) {
    return;
  }

  isProcessingRootSchedule = true;

  while (didScheduleRootProcessing) {
    didScheduleRootProcessing = false;

    FiberRoot* root = firstScheduledRoot;
    while (root != nullptr) {
      FiberRoot* const next = root->next;
      const Lanes nextLanes = getHighestPriorityPendingLanes(*root);

      if (nextLanes == NoLanes) {
        if (root->callbackNode) {
          runtime.cancelTask(root->callbackNode);
          root->callbackNode = {};
        }
        root->callbackPriority = NoLane;
        removeRootFromSchedule(*root);
      } else {
        const Lane nextLane = getHighestPriorityLane(nextLanes);
        if (!root->callbackNode || root->callbackPriority != nextLane) {
          if (root->callbackNode) {
            runtime.cancelTask(root->callbackNode);
            root->callbackNode = {};
          }
          scheduleRootTask(runtime, *root, nextLane);
        }
      }

      root = next;
    }
  }

  isProcessingRootSchedule = false;
}

void ensureScheduleProcessing(ReactRuntime& runtime) {
  if (didScheduleRootProcessing) {
    return;
  }

  didScheduleRootProcessing = true;

  if (!isProcessingRootSchedule) {
    processRootSchedule(runtime);
  }
}

} // namespace

void ensureRootIsScheduled(ReactRuntime& runtime, FiberRoot& root) {
  addRootToSchedule(root);
  ensureScheduleProcessing(runtime);
}

} // namespace react
