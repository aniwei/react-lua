#include "react-reconciler/ReactFiberRootScheduler.h"

#include "react-reconciler/ReactFiber.h"
#include "react-reconciler/ReactFiberAsyncAction.h"
#include "react-reconciler/ReactFiberLane.h"
#include "react-reconciler/ReactFiberWorkLoop.h"
#include "runtime/ReactRuntime.h"
#include "shared/ReactFeatureFlags.h"
#include <exception>
#include <iostream>

namespace react {
namespace {

RootSchedulerState& getState(ReactRuntime& runtime) {
  return runtime.rootSchedulerState();
}

const RootSchedulerState& getState(const ReactRuntime& runtime) {
  return runtime.rootSchedulerState();
}

void ensureScheduleProcessing(ReactRuntime& runtime);
void ensureScheduleIsScheduledInternal(ReactRuntime& runtime);
void scheduleImmediateRootScheduleTask(ReactRuntime& runtime);
bool performWorkOnRoot(ReactRuntime& runtime, FiberRoot& root, Lanes lanes);
void flushSyncWorkAcrossRoots(ReactRuntime& runtime, Lanes syncTransitionLanes, bool onlyLegacy);
Lanes scheduleTaskForRootDuringMicrotask(ReactRuntime& runtime, FiberRoot& root, int currentTime);
void startDefaultTransitionIndicatorIfNeeded(ReactRuntime& runtime);
void cleanupDefaultTransitionIndicatorIfNeeded(ReactRuntime& runtime, FiberRoot& root);

void reportDefaultIndicatorError(const std::exception& ex) {
  std::cerr << "React default transition indicator threw: " << ex.what() << std::endl;
}

void reportDefaultIndicatorUnknownError() {
  std::cerr << "React default transition indicator threw an unknown exception" << std::endl;
}

void startDefaultTransitionIndicatorIfNeeded(ReactRuntime& runtime) {
  if (!enableDefaultTransitionIndicator) {
    return;
  }

  startIsomorphicDefaultIndicatorIfNeeded(runtime);

  RootSchedulerState& state = getState(runtime);
  for (FiberRoot* root = state.firstScheduledRoot; root != nullptr; root = root->next) {
    if (root->indicatorLanes == NoLanes || root->pendingIndicator) {
      continue;
    }

    if (hasOngoingIsomorphicIndicator(runtime)) {
      root->pendingIndicator = retainIsomorphicIndicator(runtime);
      continue;
    }

    const auto& onIndicator = root->onDefaultTransitionIndicator;
    if (!onIndicator) {
      root->pendingIndicator = []() {};
      continue;
    }

    try {
      auto cleanup = onIndicator();
      if (cleanup) {
        root->pendingIndicator = std::move(cleanup);
      } else {
        root->pendingIndicator = []() {};
      }
    } catch (const std::exception& ex) {
      root->pendingIndicator = []() {};
      reportDefaultIndicatorError(ex);
    } catch (...) {
      root->pendingIndicator = []() {};
      reportDefaultIndicatorUnknownError();
    }
  }
}

void cleanupDefaultTransitionIndicatorIfNeeded(ReactRuntime& runtime, FiberRoot& root) {
  if (!enableDefaultTransitionIndicator) {
    return;
  }

  if (!root.pendingIndicator) {
    return;
  }

  if (root.indicatorLanes != NoLanes) {
    return;
  }

  auto cleanup = std::move(root.pendingIndicator);
  root.pendingIndicator = {};

  try {
    cleanup();
  } catch (const std::exception& ex) {
    reportDefaultIndicatorError(ex);
  } catch (...) {
    reportDefaultIndicatorUnknownError();
  }
}

void addRootToSchedule(ReactRuntime& runtime, FiberRoot& root) {
  RootSchedulerState& state = getState(runtime);
  if (&root == state.lastScheduledRoot || root.next != nullptr || state.firstScheduledRoot == &root) {
    return;
  }

  root.next = nullptr;
  if (state.lastScheduledRoot == nullptr) {
    state.firstScheduledRoot = state.lastScheduledRoot = &root;
  } else {
    state.lastScheduledRoot->next = &root;
    state.lastScheduledRoot = &root;
  }
}

void removeRootFromSchedule(ReactRuntime& runtime, FiberRoot& root) {
  RootSchedulerState& state = getState(runtime);
  FiberRoot* previous = nullptr;
  FiberRoot* current = state.firstScheduledRoot;
  while (current != nullptr) {
    if (current == &root) {
      if (previous == nullptr) {
        state.firstScheduledRoot = current->next;
      } else {
        previous->next = current->next;
      }
      if (state.lastScheduledRoot == &root) {
        state.lastScheduledRoot = previous;
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

  const int currentTime = static_cast<int>(runtime.now());
  markStarvedLanesAsExpired(root, currentTime);

  FiberRoot* const workInProgressRoot = getWorkInProgressRoot(runtime);
  const Lanes workInProgressRenderLanes =
      workInProgressRoot == &root ? getWorkInProgressRootRenderLanes(runtime) : NoLanes;
  const bool rootHasPendingCommit = root.cancelPendingCommit != nullptr || root.timeoutHandle != noTimeout;

  const Lanes lanes = getNextLanes(root, workInProgressRenderLanes, rootHasPendingCommit);
  if (lanes == NoLanes) {
    root.callbackNode = {};
    root.callbackPriority = NoLane;
    removeRootFromSchedule(runtime, root);
    return;
  }
  const bool hasRemainingWork = performWorkOnRoot(runtime, root, lanes);

  if (hasRemainingWork) {
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
  RootSchedulerState& state = getState(runtime);
  if (state.isProcessingRootSchedule) {
    return;
  }

  state.isProcessingRootSchedule = true;
  state.mightHavePendingSyncWork = false;

  while (state.didScheduleRootProcessing) {
    state.didScheduleRootProcessing = false;

    Lanes syncTransitionLanes = NoLanes;
    if (state.currentEventTransitionLane != NoLane) {
      if (runtime.shouldAttemptEagerTransition()) {
        syncTransitionLanes = state.currentEventTransitionLane;
      } else if (enableDefaultTransitionIndicator) {
        syncTransitionLanes = DefaultLane;
      }
    }

    const int currentTime = static_cast<int>(runtime.now());
    FiberRoot* prev = nullptr;
    FiberRoot* root = state.firstScheduledRoot;

    while (root != nullptr) {
      FiberRoot* const next = root->next;
      const Lanes scheduledLanes = scheduleTaskForRootDuringMicrotask(runtime, *root, currentTime);

      if (scheduledLanes == NoLanes) {
        root->next = nullptr;
        if (prev == nullptr) {
          state.firstScheduledRoot = next;
        } else {
          prev->next = next;
        }
        if (next == nullptr) {
          state.lastScheduledRoot = prev;
        }
      } else {
        prev = root;

        if ((includesSyncLane(scheduledLanes) || (enableGestureTransition && isGestureRender(scheduledLanes))) &&
            !checkIfRootIsPrerendering(*root, scheduledLanes)) {
          state.mightHavePendingSyncWork = true;
        }
      }

      root = next;
    }

    state.lastScheduledRoot = prev;

    if (!hasPendingCommitEffects(runtime)) {
      flushSyncWorkAcrossRoots(runtime, syncTransitionLanes, false);
    }
  }

  if (state.currentEventTransitionLane != NoLane) {
    state.currentEventTransitionLane = NoLane;
    startDefaultTransitionIndicatorIfNeeded(runtime);
  }

  state.isProcessingRootSchedule = false;
}

void ensureScheduleProcessing(ReactRuntime& runtime) {
  RootSchedulerState& state = getState(runtime);
  if (state.didScheduleRootProcessing) {
    return;
  }

  state.didScheduleRootProcessing = true;
  ensureScheduleIsScheduledInternal(runtime);
}

bool performWorkOnRoot(ReactRuntime& runtime, FiberRoot& root, Lanes lanes) {
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

        if (enableDefaultTransitionIndicator && includesLoadingIndicatorLanes(lanes)) {
          markIndicatorHandled(runtime, root);
        }
      }

      cleanupDefaultTransitionIndicatorIfNeeded(runtime, root);
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

  const bool hasRemainingWork = getHighestPriorityPendingLanes(root) != NoLanes;
  if (!hasRemainingWork) {
    removeRootFromSchedule(runtime, root);
  }

  return hasRemainingWork;
}

void flushSyncWorkAcrossRoots(ReactRuntime& runtime, Lanes syncTransitionLanes, bool onlyLegacy) {
  RootSchedulerState& state = getState(runtime);
  if (state.isFlushingWork) {
    return;
  }

  if (!state.mightHavePendingSyncWork && syncTransitionLanes == NoLanes) {
    return;
  }

  bool didPerformSomeWork = false;
  bool shouldProcessSchedule = false;

  state.isFlushingWork = true;
  do {
    didPerformSomeWork = false;
    FiberRoot* root = state.firstScheduledRoot;
    while (root != nullptr) {
      FiberRoot* const next = root->next;

      if (onlyLegacy && (disableLegacyMode || root->tag != RootTag::LegacyRoot)) {
        root = next;
        continue;
      }

      Lanes nextLanes = NoLanes;
      if (syncTransitionLanes != NoLanes) {
        nextLanes = getNextLanesToFlushSync(*root, syncTransitionLanes);
      } else {
        FiberRoot* const workInProgressRoot = getWorkInProgressRoot(runtime);
        const Lanes workInProgressRenderLanes =
            workInProgressRoot == root ? getWorkInProgressRootRenderLanes(runtime) : NoLanes;
        const bool rootHasPendingCommit =
            root->cancelPendingCommit != nullptr || root->timeoutHandle != noTimeout;
        nextLanes = getNextLanes(*root, workInProgressRenderLanes, rootHasPendingCommit);
      }

      if (nextLanes != NoLanes) {
        const bool shouldFlushSync =
            syncTransitionLanes != NoLanes ||
            ((!checkIfRootIsPrerendering(*root, nextLanes)) &&
             (includesSyncLane(nextLanes) || (enableGestureTransition && isGestureRender(nextLanes))));

        if (shouldFlushSync) {
          didPerformSomeWork = true;
          const bool hasRemainingWork = performWorkOnRoot(runtime, *root, nextLanes);
          if (hasRemainingWork) {
            shouldProcessSchedule = true;
          }
        }
      }

      root = next;
    }
  } while (didPerformSomeWork);

  state.isFlushingWork = false;
  state.mightHavePendingSyncWork = false;

  if (shouldProcessSchedule) {
    ensureScheduleProcessing(runtime);
  }
}

Lanes scheduleTaskForRootDuringMicrotask(ReactRuntime& runtime, FiberRoot& root, int currentTime) {
  markStarvedLanesAsExpired(root, currentTime);

  const FiberRoot* const rootWithPendingPassiveEffects = getRootWithPendingPassiveEffects(runtime);
  const Lanes pendingPassiveEffectsLanes = getPendingPassiveEffectsLanes(runtime);
  FiberRoot* const workInProgressRoot = getWorkInProgressRoot(runtime);
  const Lanes workInProgressRenderLanes =
      workInProgressRoot == &root ? getWorkInProgressRootRenderLanes(runtime) : NoLanes;
  const bool rootHasPendingCommit = root.cancelPendingCommit != nullptr || root.timeoutHandle != noTimeout;

  Lanes nextLanes = NoLanes;
  if (enableYieldingBeforePassive && rootWithPendingPassiveEffects == &root) {
    nextLanes = pendingPassiveEffectsLanes;
  } else {
    nextLanes = getNextLanes(root, workInProgressRenderLanes, rootHasPendingCommit);
  }

  const TaskHandle existingCallbackNode = root.callbackNode;
  const Lane existingCallbackPriority = root.callbackPriority;

  if (nextLanes == NoLanes ||
      (workInProgressRoot == &root && isWorkLoopSuspendedOnData(runtime)) ||
      root.cancelPendingCommit != nullptr) {
    if (existingCallbackNode) {
      runtime.cancelTask(existingCallbackNode);
    }
    root.callbackNode = {};
    root.callbackPriority = NoLane;
    return NoLanes;
  }

  if (includesSyncLane(nextLanes) && !checkIfRootIsPrerendering(root, nextLanes)) {
    if (existingCallbackNode) {
      runtime.cancelTask(existingCallbackNode);
    }
    root.callbackNode = {};
    root.callbackPriority = SyncLane;
    return nextLanes;
  }

  const Lane newCallbackPriority = getHighestPriorityLane(nextLanes);
  if (existingCallbackNode && existingCallbackPriority == newCallbackPriority) {
    return nextLanes;
  }

  if (existingCallbackNode) {
    runtime.cancelTask(existingCallbackNode);
  }

  scheduleRootTask(runtime, root, newCallbackPriority);
  return nextLanes;
}

void ensureScheduleIsScheduledInternal(ReactRuntime& runtime) {
  RootSchedulerState& state = getState(runtime);
  if (state.didScheduleMicrotask) {
    return;
  }

  state.didScheduleMicrotask = true;
  scheduleImmediateRootScheduleTask(runtime);
}

void scheduleImmediateRootScheduleTask(ReactRuntime& runtime) {
  runtime.scheduleTask(SchedulerPriority::ImmediatePriority, [&runtime]() {
    RootSchedulerState& state = getState(runtime);
    state.didScheduleMicrotask = false;
    state.didScheduleMicrotaskAct = false;
    processRootSchedule(runtime);
  });
}

} // namespace

void ensureRootIsScheduled(ReactRuntime& runtime, FiberRoot& root) {
  addRootToSchedule(runtime, root);
  getState(runtime).mightHavePendingSyncWork = true;
  ensureScheduleProcessing(runtime);
}

void flushSyncWorkOnAllRoots(ReactRuntime& runtime, Lanes syncTransitionLanes) {
  flushSyncWorkAcrossRoots(runtime, syncTransitionLanes, false);
}

void flushSyncWorkOnLegacyRootsOnly(ReactRuntime& runtime) {
  if (!disableLegacyMode) {
    flushSyncWorkAcrossRoots(runtime, NoLanes, true);
  }
}

Lane requestTransitionLane(ReactRuntime& runtime, const Transition* transition) {
  (void)transition;
  RootSchedulerState& state = getState(runtime);
  if (state.currentEventTransitionLane == NoLane) {
  const Lane actionScopeLane = peekEntangledActionLane(runtime);
    state.currentEventTransitionLane =
        actionScopeLane != NoLane ? actionScopeLane : claimNextTransitionLane();
  }
  return state.currentEventTransitionLane;
}

bool didCurrentEventScheduleTransition(ReactRuntime& runtime) {
  return getState(runtime).currentEventTransitionLane != NoLane;
}

void markIndicatorHandled(ReactRuntime& runtime, FiberRoot& root) {
  if (!enableDefaultTransitionIndicator) {
    return;
  }

  RootSchedulerState& state = getState(runtime);
  if (state.currentEventTransitionLane != NoLane) {
    root.indicatorLanes &= ~state.currentEventTransitionLane;
  }
  markIsomorphicIndicatorHandled(runtime);
}

void ensureScheduleIsScheduled(ReactRuntime& runtime) {
  ensureScheduleIsScheduledInternal(runtime);
}

void registerRootDefaultIndicator(
  ReactRuntime& runtime,
  FiberRoot& root,
  std::function<std::function<void()>()> onDefaultTransitionIndicator) {
  if (!enableDefaultTransitionIndicator) {
    root.onDefaultTransitionIndicator = {};
    return;
  }

  root.onDefaultTransitionIndicator = std::move(onDefaultTransitionIndicator);
  if (root.onDefaultTransitionIndicator) {
    registerDefaultIndicator(runtime, &root, root.onDefaultTransitionIndicator);
  }
}

} // namespace react
