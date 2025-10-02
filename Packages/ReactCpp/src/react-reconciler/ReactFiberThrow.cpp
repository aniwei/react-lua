#include "react-reconciler/ReactFiberThrow.h"

#include "react-reconciler/ReactFiber.h"
#include "react-reconciler/ReactFiberClassUpdateQueue.h"
#include "react-reconciler/ReactCapturedValue.h"
#include "react-reconciler/ReactFiberFlags.h"
#include "react-reconciler/ReactFiberOffscreenComponent.h"
#include "react-reconciler/ReactFiberSuspenseComponent.h"
#include "react-reconciler/ReactFiberSuspenseContext.h"
#include "react-reconciler/ReactFiberThenable.h"
#include "react-reconciler/ReactFiberWorkLoop.h"
#include "react-reconciler/ReactWakeable.h"
#include "react-reconciler/ReactWorkTags.h"
#include "react-reconciler/ReactRootTags.h"
#include "runtime/ReactRuntime.h"
#include "shared/ReactFeatureFlags.h"

namespace react {
namespace {

void resetSuspendedComponent(FiberNode& sourceFiber, Lanes /*rootRenderLanes*/) {
  FiberNode* const currentSourceFiber = sourceFiber.alternate;
  if (currentSourceFiber != nullptr) {
    // TODO: propagateParentContextChangesToDeferredTree once context stacking is ported.
  }

  if (!disableLegacyMode && (sourceFiber.mode & ConcurrentMode) == NoMode) {
    switch (sourceFiber.tag) {
      case WorkTag::FunctionComponent:
      case WorkTag::ForwardRef:
      case WorkTag::SimpleMemoComponent: {
        if (currentSourceFiber != nullptr) {
          sourceFiber.updateQueue = currentSourceFiber->updateQueue;
          sourceFiber.memoizedState = currentSourceFiber->memoizedState;
          sourceFiber.lanes = currentSourceFiber->lanes;
        } else {
          sourceFiber.updateQueue = nullptr;
          sourceFiber.memoizedState = nullptr;
        }
        break;
      }
      default:
        break;
    }
  }
}

RetryQueue& ensureRetryQueue(FiberNode& boundary) {
  auto* queue = static_cast<RetryQueue*>(boundary.updateQueue);
  if (queue == nullptr) {
    queue = new RetryQueue();
    boundary.updateQueue = queue;
  }
  return *queue;
}

OffscreenQueue& ensureOffscreenQueue(FiberNode& boundary) {
  auto* queue = static_cast<OffscreenQueue*>(boundary.updateQueue);
  if (queue == nullptr) {
    queue = new OffscreenQueue();
    boundary.updateQueue = queue;
  }
  return *queue;
}

RetryQueue& ensureOffscreenRetryQueue(OffscreenQueue& queue) {
  if (queue.retryQueue == nullptr) {
    queue.retryQueue = std::make_unique<RetryQueue>();
  }
  return *queue.retryQueue;
}

FiberNode* markSuspenseBoundaryShouldCapture(
    FiberNode& boundary,
    FiberNode* returnFiber,
    FiberNode& sourceFiber,
    FiberRoot& root,
    Lanes renderLanes) {
  (void)root;

  const bool isLegacyModeBoundary =
      !disableLegacyMode && (boundary.mode & ConcurrentMode) == NoMode;

  if (isLegacyModeBoundary) {
    if (&boundary == returnFiber) {
      boundary.flags = static_cast<FiberFlags>(boundary.flags | ShouldCapture);
    } else {
      boundary.flags = static_cast<FiberFlags>(boundary.flags | DidCapture);
      sourceFiber.flags = static_cast<FiberFlags>(sourceFiber.flags | ForceUpdateForLegacySuspense);
      sourceFiber.flags = static_cast<FiberFlags>(sourceFiber.flags & ~(LifecycleEffectMask | Incomplete));

      if (sourceFiber.tag == WorkTag::ClassComponent) {
        if (sourceFiber.alternate == nullptr) {
          sourceFiber.tag = WorkTag::IncompleteClassComponent;
        } else {
          // TODO: enqueue a force update once class update queues are ported.
        }
      } else if (sourceFiber.tag == WorkTag::FunctionComponent) {
        if (sourceFiber.alternate == nullptr) {
          sourceFiber.tag = WorkTag::IncompleteFunctionComponent;
        }
      }

      sourceFiber.lanes = mergeLanes(sourceFiber.lanes, SyncLane);
    }
    return &boundary;
  }

  boundary.flags = static_cast<FiberFlags>(boundary.flags | ShouldCapture);
  boundary.lanes = mergeLanes(boundary.lanes, renderLanes);
  return &boundary;
}

} // namespace

bool throwException(
    ReactRuntime& runtime,
    FiberRoot& root,
    FiberNode* returnFiber,
    FiberNode& unitOfWork,
    void* thrownValue,
    Lanes renderLanes) {
  unitOfWork.flags = static_cast<FiberFlags>(unitOfWork.flags | Incomplete);
  setWorkInProgressThrownValue(runtime, thrownValue);

  if (isWakeableValue(thrownValue)) {
    auto* const wakeable = tryGetWakeable(thrownValue);
    const bool isSuspenseyResource = isNoopSuspenseyCommitThenable(wakeable);
    resetSuspendedComponent(unitOfWork, renderLanes);

    if (FiberNode* const boundary = getSuspenseHandler()) {
      setWorkInProgressSuspendedReason(runtime, SuspendedReason::SuspendedOnData);
      switch (boundary->tag) {
        case WorkTag::SuspenseComponent:
        case WorkTag::ActivityComponent: {
          if (disableLegacyMode || (unitOfWork.mode & ConcurrentMode) != NoMode) {
            if (getShellBoundary() == nullptr) {
              renderDidSuspendDelayIfPossible(runtime);
            } else if (boundary->alternate == nullptr) {
              renderDidSuspend(runtime);
            }
          }

          boundary->flags = static_cast<FiberFlags>(boundary->flags & ~ForceClientRender);
          markSuspenseBoundaryShouldCapture(*boundary, returnFiber, unitOfWork, root, renderLanes);
          if (isSuspenseyResource) {
            boundary->flags = static_cast<FiberFlags>(boundary->flags | ScheduleRetry);
          } else {
            RetryQueue& retryQueue = ensureRetryQueue(*boundary);
            retryQueue.insert(wakeable);
            if ((disableLegacyMode || (boundary->mode & ConcurrentMode) != NoMode) &&
                !isSuspenseyResource) {
              attachPingListener(runtime, root, *wakeable, renderLanes);
            }
          }
          if (disableLegacyMode || (unitOfWork.mode & ConcurrentMode) != NoMode) {
            // renderDidSuspendDelayIfPossible or renderDidSuspend already handled above.
          } else {
            renderDidSuspend(runtime);
          }
          return false;
        }
        case WorkTag::OffscreenComponent: {
          if (disableLegacyMode || (boundary->mode & ConcurrentMode) != NoMode) {
            boundary->flags = static_cast<FiberFlags>(boundary->flags | ShouldCapture);
            if (isSuspenseyResource) {
              boundary->flags = static_cast<FiberFlags>(boundary->flags | ScheduleRetry);
            } else {
              OffscreenQueue& offscreenQueue = ensureOffscreenQueue(*boundary);
              RetryQueue& retryQueue = ensureOffscreenRetryQueue(offscreenQueue);
              retryQueue.insert(wakeable);
              attachPingListener(runtime, root, *wakeable, renderLanes);
            }
            return false;
          }
          return false;
        }
        default:
          break;
      }
    }

    if (disableLegacyMode || root.tag == RootTag::ConcurrentRoot) {
      setWorkInProgressSuspendedReason(runtime, SuspendedReason::SuspendedOnData);
      if (!isSuspenseyResource) {
        attachPingListener(runtime, root, *wakeable, renderLanes);
      }
      renderDidSuspendDelayIfPossible(runtime);
      return false;
    }

    static constexpr const char* kUncaughtSuspenseError =
        "A component suspended while responding to synchronous input. This will cause the UI to be replaced with a loading indicator. Wrap updates that suspend with startTransition.";
    thrownValue = const_cast<char*>(kUncaughtSuspenseError);
    setWorkInProgressThrownValue(runtime, thrownValue);
  }

  setWorkInProgressSuspendedReason(runtime, SuspendedReason::SuspendedOnError);
  renderDidError(runtime);

  CapturedValue errorInfo = createCapturedValueAtFiber(thrownValue, &unitOfWork);

  if (returnFiber == nullptr) {
    return true;
  }

  for (FiberNode* boundary = returnFiber; boundary != nullptr; boundary = boundary->returnFiber) {
    switch (boundary->tag) {
      case WorkTag::HostRoot: {
        boundary->flags = static_cast<FiberFlags>(boundary->flags | ShouldCapture);
        const Lane lane = pickArbitraryLane(renderLanes);
        boundary->lanes = mergeLanes(boundary->lanes, lane);

        auto* const rootStateNode = static_cast<FiberRoot*>(boundary->stateNode);
        if (rootStateNode != nullptr) {
          auto update = createRootErrorClassUpdate(*rootStateNode, errorInfo, lane);
          pushClassUpdate(*boundary, std::move(update));
        }
        return false;
      }
      case WorkTag::ClassComponent: {
        if ((boundary->flags & DidCapture) == NoFlags) {
          void* const instance = boundary->stateNode;
          if (!isAlreadyFailedLegacyErrorBoundary(instance)) {
            boundary->flags = static_cast<FiberFlags>(boundary->flags | ShouldCapture);
            const Lane lane = pickArbitraryLane(renderLanes);
            boundary->lanes = mergeLanes(boundary->lanes, lane);

            auto update = createClassErrorUpdate(lane);
            initializeClassErrorUpdate(*update, root, *boundary, errorInfo);
            pushClassUpdate(*boundary, std::move(update));
            return false;
          }
        }
        break;
      }
      default:
        break;
    }
  }

  return true;
}

} // namespace react
