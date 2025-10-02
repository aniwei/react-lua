#include "react-reconciler/ReactFiberWorkLoop.h"

#include "react-reconciler/ReactFiberConcurrentUpdates.h"
#include "react-reconciler/ReactCapturedValue.h"
#include "react-reconciler/ReactFiberErrorLogger.h"
#include "react-reconciler/ReactWakeable.h"
#include "react-reconciler/ReactFiberSuspenseContext.h"
#include "react-reconciler/ReactFiberThrow.h"
#include "react-reconciler/ReactFiberSuspenseContext.h"
#include "react-reconciler/ReactFiberRootScheduler.h"
#include "runtime/ReactRuntime.h"
#include "shared/ReactFeatureFlags.h"

#include <limits>
#include <unordered_set>
#include <utility>

namespace react {

namespace {

std::unordered_set<void*>& legacyErrorBoundariesThatAlreadyFailed() {
  static std::unordered_set<void*> instances;
  return instances;
}

void pingSuspendedRoot(
    ReactRuntime& runtime,
    FiberRoot& root,
    const Wakeable* wakeable,
    Lanes pingedLanes) {
  if (wakeable != nullptr) {
    root.pingCache.erase(wakeable);
  }

  markRootPinged(root, pingedLanes);

  FiberRoot* const workInProgressRoot = getWorkInProgressRoot(runtime);
  if (workInProgressRoot == &root) {
    const Lanes renderLanes = getWorkInProgressRootRenderLanes(runtime);
    if (isSubsetOfLanes(renderLanes, pingedLanes)) {
      const RootExitStatus exitStatus = getWorkInProgressRootExitStatus(runtime);
      const bool shouldResetStack =
          exitStatus == RootExitStatus::SuspendedWithDelay ||
          (exitStatus == RootExitStatus::Suspended &&
           includesOnlyRetries(renderLanes) &&
           (runtime.now() - getGlobalMostRecentFallbackTime(runtime)) < fallbackThrottleMs);

      if (shouldResetStack) {
        if ((getExecutionContext(runtime) & RenderContext) == NoContext) {
          prepareFreshStack(runtime, root, NoLanes);
        }
      } else {
        const Lanes accumulatedPingedLanes = mergeLanes(
            getWorkInProgressRootPingedLanes(runtime), pingedLanes);
        setWorkInProgressRootPingedLanes(runtime, accumulatedPingedLanes);
      }

      if (getWorkInProgressSuspendedRetryLanes(runtime) == renderLanes) {
        setWorkInProgressSuspendedRetryLanes(runtime, NoLanes);
      }
    }
  }

  ensureRootIsScheduled(runtime, root);
}

inline WorkLoopState& getState(ReactRuntime& runtime) {
  return runtime.workLoopState();
}

inline void cancelTimeout(TimeoutHandle) {
  // TODO: integrate host-specific timeout cancellation.
}

void resetSuspendedWorkLoopOnUnwind(FiberNode* fiber) {
  (void)fiber;
  // TODO: reset module-level state for suspended work when unwind support lands.
}

void unwindInterruptedWork(FiberNode* current, FiberNode* workInProgress, Lanes renderLanes) {
  (void)current;
  (void)workInProgress;
  (void)renderLanes;
  // TODO: implement unwind semantics once effect and context stacks are available.
}

FiberNode* completeWork(FiberNode* current, FiberNode* workInProgress, Lanes entangledRenderLanes) {
  (void)current;
  (void)workInProgress;
  (void)entangledRenderLanes;
  // TODO: port completeWork from ReactFiberCompleteWork.
  return nullptr;
}

FiberNode* unwindWork(FiberNode* current, FiberNode* workInProgress, Lanes entangledRenderLanes) {
  (void)current;
  (void)workInProgress;
  (void)entangledRenderLanes;
  // TODO: port unwindWork from ReactFiberUnwindWork.
  return nullptr;
}

void startProfilerTimer(FiberNode&) {
  // TODO: integrate ReactProfilerTimer when available.
}

void stopProfilerTimerIfRunningAndRecordIncompleteDuration(FiberNode&) {
  // TODO: integrate ReactProfilerTimer when available.
}

FiberNode* beginWork(FiberNode* /*current*/, FiberNode* workInProgress, Lanes /*entangledRenderLanes*/) {
  if (workInProgress == nullptr) {
    return nullptr;
  }
  return workInProgress->child;
}

void stopProfilerTimerIfRunningAndRecordDuration(FiberNode&) {
  // TODO: integrate ReactProfilerTimer when available.
}

bool shouldYield() {
  // TODO: integrate scheduler-based yielding.
  return false;
}

bool getIsHydrating() {
  // TODO: integrate hydration state once hydration support lands.
  return false;
}

} // namespace

bool isAlreadyFailedLegacyErrorBoundary(void* instance) {
  if (instance == nullptr) {
    return false;
  }
  const auto& instances = legacyErrorBoundariesThatAlreadyFailed();
  return instances.find(instance) != instances.end();
}

void markLegacyErrorBoundaryAsFailed(void* instance) {
  if (instance == nullptr) {
    return;
  }
  legacyErrorBoundariesThatAlreadyFailed().insert(instance);
}

void attachPingListener(
    ReactRuntime& runtime,
    FiberRoot& root,
    Wakeable& wakeable,
    Lanes lanes) {
  auto& threadIds = root.pingCache[&wakeable];
  if (!threadIds.insert(lanes).second) {
    return;
  }

  setWorkInProgressRootDidAttachPingListener(runtime, true);

  auto ping = [&runtime, &root, wakeablePtr = &wakeable, lanes]() {
    pingSuspendedRoot(runtime, root, wakeablePtr, lanes);
  };

  wakeable.then(ping, ping);
}

ExecutionContext getExecutionContext(ReactRuntime& runtime) {
  return getState(runtime).executionContext;
}

void setExecutionContext(ReactRuntime& runtime, ExecutionContext context) {
  getState(runtime).executionContext = context;
}

void pushExecutionContext(ReactRuntime& runtime, ExecutionContext context) {
  auto& state = getState(runtime);
  state.executionContext = static_cast<ExecutionContext>(state.executionContext | context);
}

void popExecutionContext(ReactRuntime& runtime, ExecutionContext context) {
  auto& state = getState(runtime);
  const auto mask = static_cast<ExecutionContext>(~context & 0xFFu);
  state.executionContext = static_cast<ExecutionContext>(state.executionContext & mask);
}

bool isAlreadyRendering(ReactRuntime& runtime) {
  const auto context = getState(runtime).executionContext;
  return (context & (RenderContext | CommitContext)) != NoContext;
}

bool isInvalidExecutionContextForEventFunction(ReactRuntime& runtime) {
  const auto context = getState(runtime).executionContext;
  return (context & RenderContext) != NoContext;
}

void setEntangledRenderLanes(ReactRuntime& runtime, Lanes lanes) {
  getState(runtime).entangledRenderLanes = lanes;
}

Lanes getEntangledRenderLanes(ReactRuntime& runtime) {
  return getState(runtime).entangledRenderLanes;
}

FiberRoot* getWorkInProgressRoot(ReactRuntime& runtime) {
  return getState(runtime).workInProgressRoot;
}

void setWorkInProgressRoot(ReactRuntime& runtime, FiberRoot* root) {
  getState(runtime).workInProgressRoot = root;
}

FiberNode* getWorkInProgressFiber(ReactRuntime& runtime) {
  return getState(runtime).workInProgressFiber;
}

void setWorkInProgressFiber(ReactRuntime& runtime, FiberNode* fiber) {
  getState(runtime).workInProgressFiber = fiber;
}

Lanes getWorkInProgressRootRenderLanes(ReactRuntime& runtime) {
  return getState(runtime).workInProgressRootRenderLanes;
}

void setWorkInProgressRootRenderLanes(ReactRuntime& runtime, Lanes lanes) {
  getState(runtime).workInProgressRootRenderLanes = lanes;
}

void* getWorkInProgressUpdateTask(ReactRuntime& runtime) {
  return getState(runtime).workInProgressUpdateTask;
}

void setWorkInProgressUpdateTask(ReactRuntime& runtime, void* task) {
  getState(runtime).workInProgressUpdateTask = task;
}

std::vector<const Transition*>& getWorkInProgressTransitions(ReactRuntime& runtime) {
  return getState(runtime).workInProgressTransitions;
}

void clearWorkInProgressTransitions(ReactRuntime& runtime) {
  getState(runtime).workInProgressTransitions.clear();
}

bool getDidIncludeCommitPhaseUpdate(ReactRuntime& runtime) {
  return getState(runtime).didIncludeCommitPhaseUpdate;
}

void setDidIncludeCommitPhaseUpdate(ReactRuntime& runtime, bool value) {
  getState(runtime).didIncludeCommitPhaseUpdate = value;
}

double getGlobalMostRecentFallbackTime(ReactRuntime& runtime) {
  return getState(runtime).globalMostRecentFallbackTime;
}

void setGlobalMostRecentFallbackTime(ReactRuntime& runtime, double value) {
  getState(runtime).globalMostRecentFallbackTime = value;
}

double getWorkInProgressRootRenderTargetTime(ReactRuntime& runtime) {
  return getState(runtime).workInProgressRootRenderTargetTime;
}

void setWorkInProgressRootRenderTargetTime(ReactRuntime& runtime, double value) {
  getState(runtime).workInProgressRootRenderTargetTime = value;
}

void* getCurrentPendingTransitionCallbacks(ReactRuntime& runtime) {
  return getState(runtime).currentPendingTransitionCallbacks;
}

void setCurrentPendingTransitionCallbacks(ReactRuntime& runtime, void* callbacks) {
  getState(runtime).currentPendingTransitionCallbacks = callbacks;
}

double getCurrentEndTime(ReactRuntime& runtime) {
  return getState(runtime).currentEndTime;
}

void setCurrentEndTime(ReactRuntime& runtime, double time) {
  getState(runtime).currentEndTime = time;
}

double getCurrentNewestExplicitSuspenseTime(ReactRuntime& runtime) {
  return getState(runtime).currentNewestExplicitSuspenseTime;
}

void setCurrentNewestExplicitSuspenseTime(ReactRuntime& runtime, double time) {
  getState(runtime).currentNewestExplicitSuspenseTime = time;
}

void markCommitTimeOfFallback(ReactRuntime& runtime) {
  setGlobalMostRecentFallbackTime(runtime, runtime.now());
}

void resetRenderTimer(ReactRuntime& runtime) {
  setWorkInProgressRootRenderTargetTime(runtime, runtime.now() + renderTimeoutMs);
}

double getRenderTargetTime(ReactRuntime& runtime) {
  return getWorkInProgressRootRenderTargetTime(runtime);
}

PendingEffectsStatus getPendingEffectsStatus(ReactRuntime& runtime) {
  return getState(runtime).pendingEffectsStatus;
}

void setPendingEffectsStatus(ReactRuntime& runtime, PendingEffectsStatus status) {
  getState(runtime).pendingEffectsStatus = status;
}

FiberRoot* getPendingEffectsRoot(ReactRuntime& runtime) {
  return getState(runtime).pendingEffectsRoot;
}

void setPendingEffectsRoot(ReactRuntime& runtime, FiberRoot* root) {
  getState(runtime).pendingEffectsRoot = root;
}

FiberNode* getPendingFinishedWork(ReactRuntime& runtime) {
  return getState(runtime).pendingFinishedWork;
}

void setPendingFinishedWork(ReactRuntime& runtime, FiberNode* fiber) {
  getState(runtime).pendingFinishedWork = fiber;
}

Lanes getPendingEffectsLanes(ReactRuntime& runtime) {
  return getState(runtime).pendingEffectsLanes;
}

void setPendingEffectsLanes(ReactRuntime& runtime, Lanes lanes) {
  getState(runtime).pendingEffectsLanes = lanes;
}

Lanes getPendingEffectsRemainingLanes(ReactRuntime& runtime) {
  return getState(runtime).pendingEffectsRemainingLanes;
}

void setPendingEffectsRemainingLanes(ReactRuntime& runtime, Lanes lanes) {
  getState(runtime).pendingEffectsRemainingLanes = lanes;
}

double getPendingEffectsRenderEndTime(ReactRuntime& runtime) {
  return getState(runtime).pendingEffectsRenderEndTime;
}

void setPendingEffectsRenderEndTime(ReactRuntime& runtime, double time) {
  getState(runtime).pendingEffectsRenderEndTime = time;
}

std::vector<const Transition*>& getPendingPassiveTransitions(ReactRuntime& runtime) {
  return getState(runtime).pendingPassiveTransitions;
}

void clearPendingPassiveTransitions(ReactRuntime& runtime) {
  getState(runtime).pendingPassiveTransitions.clear();
}

std::vector<void*>& getPendingRecoverableErrors(ReactRuntime& runtime) {
  return getState(runtime).pendingRecoverableErrors;
}

void clearPendingRecoverableErrors(ReactRuntime& runtime) {
  getState(runtime).pendingRecoverableErrors.clear();
}

void* getPendingViewTransition(ReactRuntime& runtime) {
  return getState(runtime).pendingViewTransition;
}

void setPendingViewTransition(ReactRuntime& runtime, void* transition) {
  getState(runtime).pendingViewTransition = transition;
}

std::vector<void*>& getPendingViewTransitionEvents(ReactRuntime& runtime) {
  return getState(runtime).pendingViewTransitionEvents;
}

void clearPendingViewTransitionEvents(ReactRuntime& runtime) {
  getState(runtime).pendingViewTransitionEvents.clear();
}

void* getPendingTransitionTypes(ReactRuntime& runtime) {
  return getState(runtime).pendingTransitionTypes;
}

void setPendingTransitionTypes(ReactRuntime& runtime, void* types) {
  getState(runtime).pendingTransitionTypes = types;
}

bool getPendingDidIncludeRenderPhaseUpdate(ReactRuntime& runtime) {
  return getState(runtime).pendingDidIncludeRenderPhaseUpdate;
}

void setPendingDidIncludeRenderPhaseUpdate(ReactRuntime& runtime, bool value) {
  getState(runtime).pendingDidIncludeRenderPhaseUpdate = value;
}

SuspendedCommitReason getPendingSuspendedCommitReason(ReactRuntime& runtime) {
  return getState(runtime).pendingSuspendedCommitReason;
}

void setPendingSuspendedCommitReason(ReactRuntime& runtime, SuspendedCommitReason reason) {
  getState(runtime).pendingSuspendedCommitReason = reason;
}

std::uint32_t getNestedUpdateCount(ReactRuntime& runtime) {
  return getState(runtime).nestedUpdateCount;
}

void setNestedUpdateCount(ReactRuntime& runtime, std::uint32_t count) {
  getState(runtime).nestedUpdateCount = count;
}

FiberRoot* getRootWithNestedUpdates(ReactRuntime& runtime) {
  return getState(runtime).rootWithNestedUpdates;
}

void setRootWithNestedUpdates(ReactRuntime& runtime, FiberRoot* root) {
  getState(runtime).rootWithNestedUpdates = root;
}

bool getIsFlushingPassiveEffects(ReactRuntime& runtime) {
  return getState(runtime).isFlushingPassiveEffects;
}

void setIsFlushingPassiveEffects(ReactRuntime& runtime, bool value) {
  getState(runtime).isFlushingPassiveEffects = value;
}

bool getDidScheduleUpdateDuringPassiveEffects(ReactRuntime& runtime) {
  return getState(runtime).didScheduleUpdateDuringPassiveEffects;
}

void setDidScheduleUpdateDuringPassiveEffects(ReactRuntime& runtime, bool value) {
  getState(runtime).didScheduleUpdateDuringPassiveEffects = value;
}

std::uint32_t getNestedPassiveUpdateCount(ReactRuntime& runtime) {
  return getState(runtime).nestedPassiveUpdateCount;
}

void setNestedPassiveUpdateCount(ReactRuntime& runtime, std::uint32_t count) {
  getState(runtime).nestedPassiveUpdateCount = count;
}

FiberRoot* getRootWithPassiveNestedUpdates(ReactRuntime& runtime) {
  return getState(runtime).rootWithPassiveNestedUpdates;
}

void setRootWithPassiveNestedUpdates(ReactRuntime& runtime, FiberRoot* root) {
  getState(runtime).rootWithPassiveNestedUpdates = root;
}

bool getIsRunningInsertionEffect(ReactRuntime& runtime) {
  return getState(runtime).isRunningInsertionEffect;
}

void setIsRunningInsertionEffect(ReactRuntime& runtime, bool value) {
  getState(runtime).isRunningInsertionEffect = value;
}

bool hasPendingCommitEffects(ReactRuntime& runtime) {
  const auto status = getState(runtime).pendingEffectsStatus;
  return status != PendingEffectsStatus::None && status != PendingEffectsStatus::Passive;
}

FiberRoot* getRootWithPendingPassiveEffects(ReactRuntime& runtime) {
  const auto& state = getState(runtime);
  return state.pendingEffectsStatus == PendingEffectsStatus::Passive ? state.pendingEffectsRoot : nullptr;
}

Lanes getPendingPassiveEffectsLanes(ReactRuntime& runtime) {
  return getState(runtime).pendingEffectsLanes;
}

bool isWorkLoopSuspendedOnData(ReactRuntime& runtime) {
  const auto reason = getState(runtime).suspendedReason;
  return reason == SuspendedReason::SuspendedOnData || reason == SuspendedReason::SuspendedOnAction;
}

double getCurrentTime(ReactRuntime& runtime) {
  return runtime.now();
}

void markSkippedUpdateLanes(ReactRuntime& runtime, Lanes lanes) {
  auto& state = getState(runtime);
  state.skippedLanes = mergeLanes(state.skippedLanes, lanes);
}

void renderDidSuspend(ReactRuntime& runtime) {
  auto& state = getState(runtime);
  if (state.exitStatus == RootExitStatus::InProgress) {
    state.exitStatus = RootExitStatus::Suspended;
  }
}

void renderDidSuspendDelayIfPossible(ReactRuntime& runtime) {
  auto& state = getState(runtime);
  state.exitStatus = RootExitStatus::SuspendedWithDelay;

  if (!state.didSkipSuspendedSiblings &&
      includesOnlyTransitions(state.workInProgressRootRenderLanes)) {
    state.isPrerendering = true;
  }

  const bool hasSkippedNonIdleWork =
      includesNonIdleWork(state.skippedLanes) ||
      includesNonIdleWork(state.interleavedUpdatedLanes);
  if (hasSkippedNonIdleWork && state.workInProgressRoot != nullptr) {
    constexpr bool didAttemptEntireTree = false;
    markRootSuspended(
        *state.workInProgressRoot,
        state.workInProgressRootRenderLanes,
        state.deferredLane,
        didAttemptEntireTree);
  }
}

void renderDidError(ReactRuntime& runtime) {
  auto& state = getState(runtime);
  if (state.exitStatus != RootExitStatus::SuspendedWithDelay) {
    state.exitStatus = RootExitStatus::Errored;
  }
}

void queueConcurrentError(ReactRuntime& runtime, void* error) {
  auto& state = getState(runtime);
  state.concurrentErrors.push_back(error);
}

bool renderHasNotSuspendedYet(ReactRuntime& runtime) {
  return getState(runtime).exitStatus == RootExitStatus::InProgress;
}

void markSpawnedRetryLane(ReactRuntime& runtime, Lane lane) {
  auto& state = getState(runtime);
  state.suspendedRetryLanes = mergeLanes(state.suspendedRetryLanes, lane);
}

void performUnitOfWork(ReactRuntime& runtime, FiberNode& unitOfWork) {
  auto& state = getState(runtime);
  FiberNode* const current = unitOfWork.alternate;

  FiberNode* next = nullptr;
  const bool isProfiling = enableProfilerTimer && (unitOfWork.mode & ProfileMode) != NoMode;
  if (isProfiling) {
    startProfilerTimer(unitOfWork);
  }

  next = beginWork(current, &unitOfWork, state.entangledRenderLanes);

  if (isProfiling) {
    stopProfilerTimerIfRunningAndRecordDuration(unitOfWork);
  }

  unitOfWork.memoizedProps = unitOfWork.pendingProps;
  if (next == nullptr) {
    completeUnitOfWork(runtime, unitOfWork);
  } else {
    setWorkInProgressFiber(runtime, next);
  }
}

void workLoopSync(ReactRuntime& runtime) {
  while (FiberNode* workInProgress = getWorkInProgressFiber(runtime)) {
    performUnitOfWork(runtime, *workInProgress);
  }
}

void workLoopConcurrent(ReactRuntime& runtime, bool nonIdle) {
  FiberNode* workInProgress = getWorkInProgressFiber(runtime);
  if (workInProgress == nullptr) {
    return;
  }

  const double slice = nonIdle ? 25.0 : 5.0;
  const double deadline = runtime.now() + slice;

  while ((workInProgress = getWorkInProgressFiber(runtime)) != nullptr && runtime.now() < deadline) {
    performUnitOfWork(runtime, *workInProgress);
  }
}

void workLoopConcurrentByScheduler(ReactRuntime& runtime) {
  while (FiberNode* workInProgress = getWorkInProgressFiber(runtime)) {
    if (shouldYield()) {
      break;
    }
    performUnitOfWork(runtime, *workInProgress);
  }
}

RootExitStatus renderRootSync(
    ReactRuntime& runtime,
    FiberRoot& root,
    Lanes lanes,
    bool shouldYieldForPrerendering) {
  (void)shouldYieldForPrerendering;

  pushExecutionContext(runtime, RenderContext);

  if (getWorkInProgressRoot(runtime) != &root ||
      getWorkInProgressRootRenderLanes(runtime) != lanes) {
    prepareFreshStack(runtime, root, lanes);
  }

  workLoopSync(runtime);

  const RootExitStatus exitStatus = getWorkInProgressRootExitStatus(runtime);

  if (getWorkInProgressFiber(runtime) == nullptr) {
    setWorkInProgressRoot(runtime, nullptr);
    setWorkInProgressRootRenderLanes(runtime, NoLanes);
    finishQueueingConcurrentUpdates();
  }

  popExecutionContext(runtime, RenderContext);

  return exitStatus;
}

RootExitStatus renderRootConcurrent(
    ReactRuntime& runtime,
    FiberRoot& root,
    Lanes lanes) {
  pushExecutionContext(runtime, RenderContext);

  if (getWorkInProgressRoot(runtime) != &root ||
      getWorkInProgressRootRenderLanes(runtime) != lanes) {
    prepareFreshStack(runtime, root, lanes);
  } else {
    setWorkInProgressRootIsPrerendering(runtime, checkIfRootIsPrerendering(root, lanes));
  }

  bool shouldContinue = true;
  while (shouldContinue) {
    FiberNode* workInProgress = getWorkInProgressFiber(runtime);
    if (workInProgress == nullptr) {
      break;
    }

    const SuspendedReason suspendedReason = getWorkInProgressSuspendedReason(runtime);
    if (suspendedReason != SuspendedReason::NotSuspended) {
      void* const thrownValue = getWorkInProgressThrownValue(runtime);

      switch (suspendedReason) {
        case SuspendedReason::SuspendedOnHydration:
          resetWorkInProgressStack(runtime);
          setWorkInProgressRootExitStatus(runtime, RootExitStatus::SuspendedAtTheShell);
          shouldContinue = false;
          break;
        case SuspendedReason::SuspendedOnImmediate:
          setWorkInProgressSuspendedReason(runtime, SuspendedReason::SuspendedAndReadyToContinue);
          shouldContinue = false;
          break;
        case SuspendedReason::SuspendedAndReadyToContinue:
        case SuspendedReason::SuspendedOnInstanceAndReadyToContinue:
          setWorkInProgressSuspendedReason(runtime, SuspendedReason::NotSuspended);
          setWorkInProgressThrownValue(runtime, nullptr);
          continue;
        default:
          setWorkInProgressSuspendedReason(runtime, SuspendedReason::NotSuspended);
          setWorkInProgressThrownValue(runtime, nullptr);
          throwAndUnwindWorkLoop(runtime, root, *workInProgress, thrownValue, suspendedReason);
          break;
      }

      continue;
    }

    workLoopConcurrent(runtime, includesNonIdleWork(lanes));
    shouldContinue = false;
  }

  const RootExitStatus exitStatus = getWorkInProgressRootExitStatus(runtime);

  if (getWorkInProgressFiber(runtime) == nullptr) {
    setWorkInProgressRoot(runtime, nullptr);
    setWorkInProgressRootRenderLanes(runtime, NoLanes);
    finishQueueingConcurrentUpdates();
  }

  popExecutionContext(runtime, RenderContext);

  return exitStatus;
}

void throwAndUnwindWorkLoop(
    ReactRuntime& runtime,
    FiberRoot& root,
    FiberNode& unitOfWork,
    void* thrownValue,
    SuspendedReason reason) {
  resetSuspendedWorkLoopOnUnwind(&unitOfWork);

  FiberNode* const returnFiber = unitOfWork.returnFiber;
  try {
    const bool didFatal = throwException(
        runtime,
        root,
        returnFiber,
        unitOfWork,
        thrownValue,
        getWorkInProgressRootRenderLanes(runtime));
    if (didFatal) {
      panicOnRootError(runtime, root, thrownValue);
      return;
    }
  } catch (...) {
    if (returnFiber != nullptr) {
      setWorkInProgressFiber(runtime, returnFiber);
      throw;
    }

    panicOnRootError(runtime, root, thrownValue);
    return;
  }

  if ((unitOfWork.flags & Incomplete) != NoFlags) {
    bool skipSiblings = false;

    if (getIsHydrating() || reason == SuspendedReason::SuspendedOnError) {
      skipSiblings = true;
    } else if (
        !getWorkInProgressRootIsPrerendering(runtime) &&
        !includesSomeLane(
            getWorkInProgressRootRenderLanes(runtime), OffscreenLane)) {
      skipSiblings = true;
      setWorkInProgressRootDidSkipSuspendedSiblings(runtime, true);

      if (
          reason == SuspendedReason::SuspendedOnData ||
          reason == SuspendedReason::SuspendedOnAction ||
          reason == SuspendedReason::SuspendedOnImmediate ||
          reason == SuspendedReason::SuspendedOnDeprecatedThrowPromise) {
        if (FiberNode* const boundary = getSuspenseHandler()) {
          if (boundary->tag == WorkTag::SuspenseComponent) {
            boundary->flags = static_cast<FiberFlags>(boundary->flags | ScheduleRetry);

          }
        }
      }
    }

    unwindUnitOfWork(runtime, unitOfWork, skipSiblings);
  } else {
    completeUnitOfWork(runtime, unitOfWork);
  }
}

void panicOnRootError(ReactRuntime& runtime, FiberRoot& root, void* error) {
  setWorkInProgressRootExitStatus(runtime, RootExitStatus::FatalErrored);

  CapturedValue captured{};
  if (root.current != nullptr) {
    captured = createCapturedValueAtFiber(error, root.current);
  } else {
    captured = createCapturedValueFromError(error, std::string{});
  }

  logUncaughtError(root, captured);
  setWorkInProgressFiber(runtime, nullptr);
}

void completeUnitOfWork(ReactRuntime& runtime, FiberNode& unitOfWork) {
  auto& state = getState(runtime);
  FiberNode* completedWork = &unitOfWork;

  do {
    if ((completedWork->flags & Incomplete) != NoFlags) {
      const bool skipSiblings = state.didSkipSuspendedSiblings;
      unwindUnitOfWork(runtime, *completedWork, skipSiblings);
      return;
    }

    FiberNode* current = completedWork->alternate;
    FiberNode* returnFiber = completedWork->returnFiber;

    startProfilerTimer(*completedWork);
    FiberNode* next = completeWork(current, completedWork, state.entangledRenderLanes);
    if (enableProfilerTimer && (completedWork->mode & ProfileMode) != NoMode) {
      stopProfilerTimerIfRunningAndRecordIncompleteDuration(*completedWork);
    }

    if (next != nullptr) {
      setWorkInProgressFiber(runtime, next);
      return;
    }

    FiberNode* siblingFiber = completedWork->sibling;
    if (siblingFiber != nullptr) {
      setWorkInProgressFiber(runtime, siblingFiber);
      return;
    }

    completedWork = returnFiber;
    setWorkInProgressFiber(runtime, completedWork);
  } while (completedWork != nullptr);

  if (state.exitStatus == RootExitStatus::InProgress) {
    setWorkInProgressRootExitStatus(runtime, RootExitStatus::Completed);
  }
}

void unwindUnitOfWork(ReactRuntime& runtime, FiberNode& unitOfWork, bool skipSiblings) {
  auto& state = getState(runtime);
  FiberNode* incompleteWork = &unitOfWork;

  do {
    FiberNode* current = incompleteWork->alternate;
    FiberNode* next = unwindWork(current, incompleteWork, state.entangledRenderLanes);

    if (next != nullptr) {
      next->flags &= HostEffectMask;
      setWorkInProgressFiber(runtime, next);
      return;
    }

    if (enableProfilerTimer && (incompleteWork->mode & ProfileMode) != NoMode) {
      stopProfilerTimerIfRunningAndRecordIncompleteDuration(*incompleteWork);
      double actualDuration = incompleteWork->actualDuration;
      for (FiberNode* child = incompleteWork->child; child != nullptr; child = child->sibling) {
        actualDuration += child->actualDuration;
      }
      incompleteWork->actualDuration = actualDuration;
    }

    FiberNode* returnFiber = incompleteWork->returnFiber;
    if (returnFiber != nullptr) {
      returnFiber->flags |= Incomplete;
      returnFiber->subtreeFlags = NoFlags;
      returnFiber->deletions.clear();
    }

    if (!skipSiblings) {
      FiberNode* siblingFiber = incompleteWork->sibling;
      if (siblingFiber != nullptr) {
        setWorkInProgressFiber(runtime, siblingFiber);
        return;
      }
    }

    incompleteWork = returnFiber;
    setWorkInProgressFiber(runtime, incompleteWork);
  } while (incompleteWork != nullptr);

  setWorkInProgressRootExitStatus(runtime, RootExitStatus::SuspendedAtTheShell);
  setWorkInProgressFiber(runtime, nullptr);
}

FiberNode* prepareFreshStack(ReactRuntime& runtime, FiberRoot& root, Lanes lanes) {
  if (root.timeoutHandle != noTimeout) {
    cancelTimeout(root.timeoutHandle);
    root.timeoutHandle = noTimeout;
  }

  if (root.cancelPendingCommit) {
    auto cancel = std::move(root.cancelPendingCommit);
    root.cancelPendingCommit = nullptr;
    cancel();
  }

  resetWorkInProgressStack(runtime);

  setWorkInProgressRoot(runtime, &root);
  FiberNode* rootWorkInProgress = createWorkInProgress(root.current, nullptr);
  setWorkInProgressFiber(runtime, rootWorkInProgress);
  setWorkInProgressRootRenderLanes(runtime, lanes);
  setWorkInProgressSuspendedReason(runtime, SuspendedReason::NotSuspended);
  setWorkInProgressThrownValue(runtime, nullptr);
  setWorkInProgressRootDidSkipSuspendedSiblings(runtime, false);
  setWorkInProgressRootIsPrerendering(runtime, checkIfRootIsPrerendering(root, lanes));
  setWorkInProgressRootDidAttachPingListener(runtime, false);
  setWorkInProgressRootExitStatus(runtime, RootExitStatus::InProgress);
  setWorkInProgressRootSkippedLanes(runtime, NoLanes);
  setWorkInProgressRootInterleavedUpdatedLanes(runtime, NoLanes);
  setWorkInProgressRootRenderPhaseUpdatedLanes(runtime, NoLanes);
  setWorkInProgressRootPingedLanes(runtime, NoLanes);
  setWorkInProgressDeferredLane(runtime, NoLane);
  setWorkInProgressSuspendedRetryLanes(runtime, NoLanes);
  clearWorkInProgressRootConcurrentErrors(runtime);
  clearWorkInProgressRootRecoverableErrors(runtime);
  setWorkInProgressRootDidIncludeRecursiveRenderUpdate(runtime, false);
  setWorkInProgressUpdateTask(runtime, nullptr);
  clearWorkInProgressTransitions(runtime);
  setDidIncludeCommitPhaseUpdate(runtime, false);
  setCurrentPendingTransitionCallbacks(runtime, nullptr);
  setCurrentEndTime(runtime, 0.0);
  setWorkInProgressRootRenderTargetTime(runtime, std::numeric_limits<double>::infinity());

  setEntangledRenderLanes(runtime, getEntangledLanes(root, lanes));

  finishQueueingConcurrentUpdates();

  return rootWorkInProgress;
}

void resetWorkInProgressStack(ReactRuntime& runtime) {
  FiberNode* const workInProgress = getWorkInProgressFiber(runtime);
  if (workInProgress == nullptr) {
    return;
  }

  FiberNode* interruptedWork = nullptr;
  if (getWorkInProgressSuspendedReason(runtime) == SuspendedReason::NotSuspended) {
    interruptedWork = workInProgress->returnFiber;
  } else {
    resetSuspendedWorkLoopOnUnwind(workInProgress);
    interruptedWork = workInProgress;
  }

  const Lanes renderLanes = getWorkInProgressRootRenderLanes(runtime);
  while (interruptedWork != nullptr) {
    FiberNode* current = interruptedWork->alternate;
    unwindInterruptedWork(current, interruptedWork, renderLanes);
    interruptedWork = interruptedWork->returnFiber;
  }

  setWorkInProgressFiber(runtime, nullptr);
}

SuspendedReason getWorkInProgressSuspendedReason(ReactRuntime& runtime) {
  return getState(runtime).suspendedReason;
}

void setWorkInProgressSuspendedReason(ReactRuntime& runtime, SuspendedReason reason) {
  getState(runtime).suspendedReason = reason;
}

void* getWorkInProgressThrownValue(ReactRuntime& runtime) {
  return getState(runtime).thrownValue;
}

void setWorkInProgressThrownValue(ReactRuntime& runtime, void* value) {
  getState(runtime).thrownValue = value;
}

bool getWorkInProgressRootDidSkipSuspendedSiblings(ReactRuntime& runtime) {
  return getState(runtime).didSkipSuspendedSiblings;
}

void setWorkInProgressRootDidSkipSuspendedSiblings(ReactRuntime& runtime, bool value) {
  getState(runtime).didSkipSuspendedSiblings = value;
}

bool getWorkInProgressRootIsPrerendering(ReactRuntime& runtime) {
  return getState(runtime).isPrerendering;
}

void setWorkInProgressRootIsPrerendering(ReactRuntime& runtime, bool value) {
  getState(runtime).isPrerendering = value;
}

bool getWorkInProgressRootDidAttachPingListener(ReactRuntime& runtime) {
  return getState(runtime).didAttachPingListener;
}

void setWorkInProgressRootDidAttachPingListener(ReactRuntime& runtime, bool value) {
  getState(runtime).didAttachPingListener = value;
}

RootExitStatus getWorkInProgressRootExitStatus(ReactRuntime& runtime) {
  return getState(runtime).exitStatus;
}

void setWorkInProgressRootExitStatus(ReactRuntime& runtime, RootExitStatus status) {
  getState(runtime).exitStatus = status;
}

Lanes getWorkInProgressRootSkippedLanes(ReactRuntime& runtime) {
  return getState(runtime).skippedLanes;
}

void setWorkInProgressRootSkippedLanes(ReactRuntime& runtime, Lanes lanes) {
  getState(runtime).skippedLanes = lanes;
}

Lanes getWorkInProgressRootInterleavedUpdatedLanes(ReactRuntime& runtime) {
  return getState(runtime).interleavedUpdatedLanes;
}

void setWorkInProgressRootInterleavedUpdatedLanes(ReactRuntime& runtime, Lanes lanes) {
  getState(runtime).interleavedUpdatedLanes = lanes;
}

Lanes getWorkInProgressRootRenderPhaseUpdatedLanes(ReactRuntime& runtime) {
  return getState(runtime).renderPhaseUpdatedLanes;
}

void setWorkInProgressRootRenderPhaseUpdatedLanes(ReactRuntime& runtime, Lanes lanes) {
  getState(runtime).renderPhaseUpdatedLanes = lanes;
}

Lanes getWorkInProgressRootPingedLanes(ReactRuntime& runtime) {
  return getState(runtime).pingedLanes;
}

void setWorkInProgressRootPingedLanes(ReactRuntime& runtime, Lanes lanes) {
  getState(runtime).pingedLanes = lanes;
}

Lane getWorkInProgressDeferredLane(ReactRuntime& runtime) {
  return getState(runtime).deferredLane;
}

void setWorkInProgressDeferredLane(ReactRuntime& runtime, Lane lane) {
  getState(runtime).deferredLane = lane;
}

Lanes getWorkInProgressSuspendedRetryLanes(ReactRuntime& runtime) {
  return getState(runtime).suspendedRetryLanes;
}

void setWorkInProgressSuspendedRetryLanes(ReactRuntime& runtime, Lanes lanes) {
  getState(runtime).suspendedRetryLanes = lanes;
}

std::vector<void*>& getWorkInProgressRootConcurrentErrors(ReactRuntime& runtime) {
  return getState(runtime).concurrentErrors;
}

void clearWorkInProgressRootConcurrentErrors(ReactRuntime& runtime) {
  getState(runtime).concurrentErrors.clear();
}

std::vector<void*>& getWorkInProgressRootRecoverableErrors(ReactRuntime& runtime) {
  return getState(runtime).recoverableErrors;
}

void clearWorkInProgressRootRecoverableErrors(ReactRuntime& runtime) {
  getState(runtime).recoverableErrors.clear();
}

bool getWorkInProgressRootDidIncludeRecursiveRenderUpdate(ReactRuntime& runtime) {
  return getState(runtime).didIncludeRecursiveRenderUpdate;
}

void setWorkInProgressRootDidIncludeRecursiveRenderUpdate(ReactRuntime& runtime, bool value) {
  getState(runtime).didIncludeRecursiveRenderUpdate = value;
}

} // namespace react
