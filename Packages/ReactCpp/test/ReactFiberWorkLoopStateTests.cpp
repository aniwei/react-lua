#include "react-reconciler/ReactFiberWorkLoop.h"
#include "runtime/ReactRuntime.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace react::test {

namespace {

void resetState(ReactRuntime& runtime) {
  runtime.resetWorkLoop();
  setExecutionContext(runtime, NoContext);
  resetWorkInProgressStack(runtime);
  setWorkInProgressRootRenderLanes(runtime, NoLanes);
  setWorkInProgressSuspendedReason(runtime, SuspendedReason::NotSuspended);
  setWorkInProgressThrownValue(runtime, nullptr);
  setWorkInProgressRootDidSkipSuspendedSiblings(runtime, false);
  setWorkInProgressRootIsPrerendering(runtime, false);
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
  setPendingEffectsStatus(runtime, PendingEffectsStatus::None);
  setPendingEffectsRoot(runtime, nullptr);
  setPendingFinishedWork(runtime, nullptr);
  setPendingEffectsLanes(runtime, NoLanes);
  setPendingEffectsRemainingLanes(runtime, NoLanes);
  setPendingEffectsRenderEndTime(runtime, -0.0);
  clearPendingPassiveTransitions(runtime);
  clearPendingRecoverableErrors(runtime);
  setPendingViewTransition(runtime, nullptr);
  clearPendingViewTransitionEvents(runtime);
  setPendingTransitionTypes(runtime, nullptr);
  setPendingDidIncludeRenderPhaseUpdate(runtime, false);
  setPendingSuspendedCommitReason(runtime, SuspendedCommitReason::ImmediateCommit);
  setNestedUpdateCount(runtime, 0);
  setRootWithNestedUpdates(runtime, nullptr);
  setIsFlushingPassiveEffects(runtime, false);
  setDidScheduleUpdateDuringPassiveEffects(runtime, false);
  setNestedPassiveUpdateCount(runtime, 0);
  setRootWithPassiveNestedUpdates(runtime, nullptr);
  setIsRunningInsertionEffect(runtime, false);
}

} // namespace

bool runReactFiberWorkLoopStateTests() {
  ReactRuntime runtime;
  // renderRootConcurrent should mirror sync behavior when the scheduler never yields.
  resetState(runtime);

  // Execution context bitmask operations
  assert(getExecutionContext(runtime) == NoContext);
  pushExecutionContext(runtime, BatchedContext);
  assert(getExecutionContext(runtime) == BatchedContext);
  pushExecutionContext(runtime, RenderContext);
  assert((getExecutionContext(runtime) & RenderContext) == RenderContext);
  assert((getExecutionContext(runtime) & BatchedContext) == BatchedContext);
  assert(isAlreadyRendering(runtime));
  assert(isInvalidExecutionContextForEventFunction(runtime));
  popExecutionContext(runtime, BatchedContext);
  assert((getExecutionContext(runtime) & BatchedContext) == 0);
  setExecutionContext(runtime, CommitContext);
  assert(getExecutionContext(runtime) == CommitContext);
  assert(isAlreadyRendering(runtime));
  assert(!isInvalidExecutionContextForEventFunction(runtime));
  setExecutionContext(runtime, NoContext);
  assert(!isAlreadyRendering(runtime));

  // Work-in-progress root/fiber/lanes bookkeeping
  FiberRoot root{};
  setWorkInProgressRoot(runtime, &root);
  assert(getWorkInProgressRoot(runtime) == &root);

  root.pendingLanes = DefaultLane;
  root.suspendedLanes = NoLanes;
  root.pingedLanes = NoLanes;
  assert(!checkIfRootIsPrerendering(root, DefaultLane));
  root.suspendedLanes = DefaultLane;
  assert(checkIfRootIsPrerendering(root, DefaultLane));
  root.pingedLanes = DefaultLane;
  assert(!checkIfRootIsPrerendering(root, DefaultLane));
  root.pendingLanes = NoLanes;
  assert(checkIfRootIsPrerendering(root, DefaultLane));

  FiberNode* fiber = createFiber(WorkTag::HostRoot);
  setWorkInProgressFiber(runtime, fiber);
  assert(getWorkInProgressFiber(runtime) == fiber);

  setWorkInProgressRootRenderLanes(runtime, DefaultLane);
  assert(getWorkInProgressRootRenderLanes(runtime) == DefaultLane);

  setEntangledRenderLanes(runtime, TransitionLane1 | TransitionLane2);
  assert(getEntangledRenderLanes(runtime) == (TransitionLane1 | TransitionLane2));
  setEntangledRenderLanes(runtime, NoLanes);

  // Work loop root state bookkeeping
  assert(getWorkInProgressSuspendedReason(runtime) == SuspendedReason::NotSuspended);
  setWorkInProgressSuspendedReason(runtime, SuspendedReason::SuspendedOnImmediate);
  assert(getWorkInProgressSuspendedReason(runtime) == SuspendedReason::SuspendedOnImmediate);
  setWorkInProgressSuspendedReason(runtime, SuspendedReason::NotSuspended);

  assert(getWorkInProgressThrownValue(runtime) == nullptr);
  void* thrown = reinterpret_cast<void*>(0x1);
  setWorkInProgressThrownValue(runtime, thrown);
  assert(getWorkInProgressThrownValue(runtime) == thrown);
  setWorkInProgressThrownValue(runtime, nullptr);

  assert(!getWorkInProgressRootDidSkipSuspendedSiblings(runtime));
  setWorkInProgressRootDidSkipSuspendedSiblings(runtime, true);
  assert(getWorkInProgressRootDidSkipSuspendedSiblings(runtime));
  setWorkInProgressRootDidSkipSuspendedSiblings(runtime, false);

  assert(!getWorkInProgressRootIsPrerendering(runtime));
  setWorkInProgressRootIsPrerendering(runtime, true);
  assert(getWorkInProgressRootIsPrerendering(runtime));
  setWorkInProgressRootIsPrerendering(runtime, false);

  assert(!getWorkInProgressRootDidAttachPingListener(runtime));
  setWorkInProgressRootDidAttachPingListener(runtime, true);
  assert(getWorkInProgressRootDidAttachPingListener(runtime));
  setWorkInProgressRootDidAttachPingListener(runtime, false);

  assert(getWorkInProgressRootExitStatus(runtime) == RootExitStatus::InProgress);
  setWorkInProgressRootExitStatus(runtime, RootExitStatus::Completed);
  assert(getWorkInProgressRootExitStatus(runtime) == RootExitStatus::Completed);
  setWorkInProgressRootExitStatus(runtime, RootExitStatus::InProgress);
  assert(renderHasNotSuspendedYet(runtime));
  renderDidSuspend(runtime);
  assert(getWorkInProgressRootExitStatus(runtime) == RootExitStatus::Suspended);
  assert(!renderHasNotSuspendedYet(runtime));
  setWorkInProgressRootExitStatus(runtime, RootExitStatus::Completed);
  renderDidSuspend(runtime);
  assert(getWorkInProgressRootExitStatus(runtime) == RootExitStatus::Completed);
  setWorkInProgressRootExitStatus(runtime, RootExitStatus::InProgress);
  renderDidError(runtime);
  assert(getWorkInProgressRootExitStatus(runtime) == RootExitStatus::Errored);
  setWorkInProgressRootExitStatus(runtime, RootExitStatus::SuspendedWithDelay);
  renderDidError(runtime);
  assert(getWorkInProgressRootExitStatus(runtime) == RootExitStatus::SuspendedWithDelay);
  setWorkInProgressRootExitStatus(runtime, RootExitStatus::InProgress);
  setWorkInProgressRootDidSkipSuspendedSiblings(runtime, false);
  setWorkInProgressRootRenderLanes(runtime, TransitionLane1);
  setWorkInProgressRootSkippedLanes(runtime, DefaultLane);
  setWorkInProgressRootInterleavedUpdatedLanes(runtime, NoLanes);
  setWorkInProgressDeferredLane(runtime, NoLane);
  renderDidSuspendDelayIfPossible(runtime);
  assert(getWorkInProgressRootExitStatus(runtime) == RootExitStatus::SuspendedWithDelay);
  assert(getWorkInProgressRootIsPrerendering(runtime));
  assert((root.suspendedLanes & TransitionLane1) == TransitionLane1);
  setWorkInProgressRootIsPrerendering(runtime, false);
  setWorkInProgressRootExitStatus(runtime, RootExitStatus::InProgress);
  setWorkInProgressRootSkippedLanes(runtime, NoLanes);
  setWorkInProgressRootInterleavedUpdatedLanes(runtime, NoLanes);
  root.suspendedLanes = NoLanes;

  assert(getWorkInProgressRootSkippedLanes(runtime) == NoLanes);
  setWorkInProgressRootSkippedLanes(runtime, TransitionLane3);
  assert(getWorkInProgressRootSkippedLanes(runtime) == TransitionLane3);
  setWorkInProgressRootSkippedLanes(runtime, NoLanes);
  markSkippedUpdateLanes(runtime, TransitionLane1);
  assert(getWorkInProgressRootSkippedLanes(runtime) == TransitionLane1);
  markSkippedUpdateLanes(runtime, TransitionLane2);
  assert(getWorkInProgressRootSkippedLanes(runtime) == (TransitionLane1 | TransitionLane2));
  setWorkInProgressRootSkippedLanes(runtime, NoLanes);

  assert(getWorkInProgressRootInterleavedUpdatedLanes(runtime) == NoLanes);
  setWorkInProgressRootInterleavedUpdatedLanes(runtime, TransitionLane4);
  assert(getWorkInProgressRootInterleavedUpdatedLanes(runtime) == TransitionLane4);
  setWorkInProgressRootInterleavedUpdatedLanes(runtime, NoLanes);

  assert(getWorkInProgressRootRenderPhaseUpdatedLanes(runtime) == NoLanes);
  setWorkInProgressRootRenderPhaseUpdatedLanes(runtime, TransitionLane5);
  assert(getWorkInProgressRootRenderPhaseUpdatedLanes(runtime) == TransitionLane5);
  setWorkInProgressRootRenderPhaseUpdatedLanes(runtime, NoLanes);

  assert(getWorkInProgressRootPingedLanes(runtime) == NoLanes);
  setWorkInProgressRootPingedLanes(runtime, TransitionLane6);
  assert(getWorkInProgressRootPingedLanes(runtime) == TransitionLane6);
  setWorkInProgressRootPingedLanes(runtime, NoLanes);

  assert(getWorkInProgressDeferredLane(runtime) == NoLane);
  setWorkInProgressDeferredLane(runtime, DeferredLane);
  assert(getWorkInProgressDeferredLane(runtime) == DeferredLane);
  setWorkInProgressDeferredLane(runtime, NoLane);

  assert(getWorkInProgressSuspendedRetryLanes(runtime) == NoLanes);
  setWorkInProgressSuspendedRetryLanes(runtime, RetryLane1 | RetryLane2);
  assert(getWorkInProgressSuspendedRetryLanes(runtime) == (RetryLane1 | RetryLane2));
  setWorkInProgressSuspendedRetryLanes(runtime, NoLanes);
  markSpawnedRetryLane(runtime, RetryLane3);
  assert(getWorkInProgressSuspendedRetryLanes(runtime) == RetryLane3);
  markSpawnedRetryLane(runtime, RetryLane4);
  assert(getWorkInProgressSuspendedRetryLanes(runtime) == (RetryLane3 | RetryLane4));
  setWorkInProgressSuspendedRetryLanes(runtime, NoLanes);

  assert(getWorkInProgressRootConcurrentErrors(runtime).empty());
  queueConcurrentError(runtime, reinterpret_cast<void*>(0x2));
  assert(getWorkInProgressRootConcurrentErrors(runtime).size() == 1);
  clearWorkInProgressRootConcurrentErrors(runtime);
  assert(getWorkInProgressRootConcurrentErrors(runtime).empty());

  assert(getWorkInProgressRootRecoverableErrors(runtime).empty());
  auto& recoverableErrors = getWorkInProgressRootRecoverableErrors(runtime);
  recoverableErrors.push_back(reinterpret_cast<void*>(0x3));
  assert(recoverableErrors.size() == 1);
  clearWorkInProgressRootRecoverableErrors(runtime);
  assert(getWorkInProgressRootRecoverableErrors(runtime).empty());

  assert(!getWorkInProgressRootDidIncludeRecursiveRenderUpdate(runtime));
  setWorkInProgressRootDidIncludeRecursiveRenderUpdate(runtime, true);
  assert(getWorkInProgressRootDidIncludeRecursiveRenderUpdate(runtime));
  setWorkInProgressRootDidIncludeRecursiveRenderUpdate(runtime, false);

  assert(getGlobalMostRecentFallbackTime(runtime) == 0.0);
  markCommitTimeOfFallback(runtime);
  assert(getGlobalMostRecentFallbackTime(runtime) > 0.0);

  assert(getCurrentNewestExplicitSuspenseTime(runtime) == 0.0);
  setCurrentNewestExplicitSuspenseTime(runtime, 42.0);
  assert(getCurrentNewestExplicitSuspenseTime(runtime) == 42.0);
  setCurrentNewestExplicitSuspenseTime(runtime, 0.0);

  setWorkInProgressRootRenderTargetTime(runtime, 0.0);
  const double beforeReset = runtime.now();
  resetRenderTimer(runtime);
  const double targetTime = getRenderTargetTime(runtime);
  const double delta = targetTime - beforeReset;
  assert(delta > renderTimeoutMs - 10.0);
  assert(delta < renderTimeoutMs + 10.0);

  assert(getPendingEffectsStatus(runtime) == PendingEffectsStatus::None);
  assert(!hasPendingCommitEffects(runtime));
  setPendingEffectsStatus(runtime, PendingEffectsStatus::Mutation);
  assert(hasPendingCommitEffects(runtime));
  setPendingEffectsStatus(runtime, PendingEffectsStatus::Passive);
  assert(!hasPendingCommitEffects(runtime));
  setPendingEffectsStatus(runtime, PendingEffectsStatus::None);

  assert(getRootWithPendingPassiveEffects(runtime) == nullptr);
  FiberRoot passiveEffectsRoot{};
  setPendingEffectsRoot(runtime, &passiveEffectsRoot);
  setPendingEffectsStatus(runtime, PendingEffectsStatus::Passive);
  assert(getRootWithPendingPassiveEffects(runtime) == &passiveEffectsRoot);
  setPendingEffectsStatus(runtime, PendingEffectsStatus::None);
  setPendingEffectsRoot(runtime, nullptr);

  assert(getPendingPassiveEffectsLanes(runtime) == NoLanes);
  setPendingEffectsLanes(runtime, DefaultLane);
  assert(getPendingPassiveEffectsLanes(runtime) == DefaultLane);
  setPendingEffectsLanes(runtime, NoLanes);

  assert(!isWorkLoopSuspendedOnData(runtime));
  setWorkInProgressSuspendedReason(runtime, SuspendedReason::SuspendedOnData);
  assert(isWorkLoopSuspendedOnData(runtime));
  setWorkInProgressSuspendedReason(runtime, SuspendedReason::SuspendedOnAction);
  assert(isWorkLoopSuspendedOnData(runtime));
  setWorkInProgressSuspendedReason(runtime, SuspendedReason::NotSuspended);

  const double currentTime = getCurrentTime(runtime);
  assert(currentTime >= beforeReset);

  // prepareFreshStack initializes render state for a new root
  setWorkInProgressRootDidSkipSuspendedSiblings(runtime, true);
  setWorkInProgressRootInterleavedUpdatedLanes(runtime, TransitionLane1);
  setWorkInProgressRootRenderPhaseUpdatedLanes(runtime, TransitionLane2);
  setWorkInProgressRootPingedLanes(runtime, TransitionLane3);
  setWorkInProgressDeferredLane(runtime, DeferredLane);
  setWorkInProgressSuspendedRetryLanes(runtime, RetryLane1);
  setWorkInProgressRootDidIncludeRecursiveRenderUpdate(runtime, true);
  auto& existingConcurrentErrors = getWorkInProgressRootConcurrentErrors(runtime);
  existingConcurrentErrors.push_back(reinterpret_cast<void*>(0x4));
  auto& existingRecoverableErrors = getWorkInProgressRootRecoverableErrors(runtime);
  existingRecoverableErrors.push_back(reinterpret_cast<void*>(0x5));
  setEntangledRenderLanes(runtime, TransitionLane4);

  FiberRoot prepareRoot{};
  FiberNode* currentRootFiber = createFiber(WorkTag::HostRoot);
  prepareRoot.current = currentRootFiber;
  prepareRoot.pendingLanes = DefaultLane;
  prepareRoot.suspendedLanes = NoLanes;
  prepareRoot.pingedLanes = NoLanes;
  prepareRoot.timeoutHandle = 42;
  bool cancelPendingCommitCalled = false;
  prepareRoot.cancelPendingCommit = [&]() {
    cancelPendingCommitCalled = true;
  };

  const Lanes renderLanes = DefaultLane;
  const bool expectedPrerendering = checkIfRootIsPrerendering(prepareRoot, renderLanes);
  const Lanes expectedEntangled = getEntangledLanes(prepareRoot, renderLanes);

  FiberNode* rootWorkInProgress = prepareFreshStack(runtime, prepareRoot, renderLanes);

  assert(rootWorkInProgress != nullptr);
  assert(getWorkInProgressRoot(runtime) == &prepareRoot);
  assert(getWorkInProgressFiber(runtime) == rootWorkInProgress);
  assert(getWorkInProgressRootRenderLanes(runtime) == renderLanes);
  assert(getWorkInProgressSuspendedReason(runtime) == SuspendedReason::NotSuspended);
  assert(getWorkInProgressThrownValue(runtime) == nullptr);
  assert(!getWorkInProgressRootDidSkipSuspendedSiblings(runtime));
  assert(getWorkInProgressRootIsPrerendering(runtime) == expectedPrerendering);
  assert(!getWorkInProgressRootDidAttachPingListener(runtime));
  assert(getWorkInProgressRootExitStatus(runtime) == RootExitStatus::InProgress);
  assert(getWorkInProgressRootSkippedLanes(runtime) == NoLanes);
  assert(getWorkInProgressRootInterleavedUpdatedLanes(runtime) == NoLanes);
  assert(getWorkInProgressRootRenderPhaseUpdatedLanes(runtime) == NoLanes);
  assert(getWorkInProgressRootPingedLanes(runtime) == NoLanes);
  assert(getWorkInProgressDeferredLane(runtime) == NoLane);
  assert(getWorkInProgressSuspendedRetryLanes(runtime) == NoLanes);
  assert(getWorkInProgressRootConcurrentErrors(runtime).empty());
  assert(getWorkInProgressRootRecoverableErrors(runtime).empty());
  assert(!getWorkInProgressRootDidIncludeRecursiveRenderUpdate(runtime));
  assert(getEntangledRenderLanes(runtime) == expectedEntangled);
  assert(getWorkInProgressUpdateTask(runtime) == nullptr);
  assert(getWorkInProgressTransitions(runtime).empty());
  assert(!getDidIncludeCommitPhaseUpdate(runtime));
  assert(getCurrentPendingTransitionCallbacks(runtime) == nullptr);
  assert(getCurrentEndTime(runtime) == 0.0);
  assert(getWorkInProgressRootRenderTargetTime(runtime) == std::numeric_limits<double>::infinity());
  assert(prepareRoot.timeoutHandle == noTimeout);
  assert(cancelPendingCommitCalled);
  assert(!prepareRoot.cancelPendingCommit);

  // Reset with suspended reason branch to ensure guard coverage.
  setWorkInProgressSuspendedReason(runtime, SuspendedReason::SuspendedOnError);
  resetWorkInProgressStack(runtime);
  assert(getWorkInProgressRoot(runtime) == &prepareRoot);
  assert(getWorkInProgressFiber(runtime) == nullptr);
  assert(getWorkInProgressRootRenderLanes(runtime) == renderLanes);

  delete rootWorkInProgress;
  delete currentRootFiber;

  // Reset when no work-in-progress fiber is present should be a no-op.
  resetWorkInProgressStack(runtime);
  assert(getWorkInProgressRoot(runtime) == &prepareRoot);
  assert(getWorkInProgressFiber(runtime) == nullptr);
  assert(getWorkInProgressRootRenderLanes(runtime) == renderLanes);

  delete fiber;

  // panicOnRootError sets fatal exit status and clears work-in-progress fiber.
  FiberRoot panicRoot{};
  FiberNode* panicCurrent = createFiber(WorkTag::HostRoot);
  panicRoot.current = panicCurrent;
  setWorkInProgressRootExitStatus(runtime, RootExitStatus::InProgress);
  setWorkInProgressFiber(runtime, panicCurrent);
  panicOnRootError(runtime, panicRoot, reinterpret_cast<void*>(0x6));
  assert(getWorkInProgressRootExitStatus(runtime) == RootExitStatus::FatalErrored);
  assert(getWorkInProgressFiber(runtime) == nullptr);
  delete panicCurrent;

  // renderRootSync prepares a fresh stack and completes the tree synchronously.
  FiberRoot renderRoot{};
  FiberNode* renderCurrent = createFiber(WorkTag::HostRoot);
  renderRoot.current = renderCurrent;
  renderRoot.pendingLanes = DefaultLane;
  renderRoot.suspendedLanes = NoLanes;
  renderRoot.pingedLanes = NoLanes;

  FiberNode* renderChild = createFiber(WorkTag::FunctionComponent);
  renderCurrent->child = renderChild;
  renderChild->returnFiber = renderCurrent;

  setWorkInProgressRoot(runtime, nullptr);
  setWorkInProgressFiber(runtime, nullptr);
  setWorkInProgressRootRenderLanes(runtime, NoLanes);
  setWorkInProgressRootExitStatus(runtime, RootExitStatus::InProgress);

  const RootExitStatus renderExit = renderRootSync(runtime, renderRoot, DefaultLane, false);
  assert(renderExit == RootExitStatus::Completed);
  assert(getWorkInProgressRoot(runtime) == nullptr);
  assert(getWorkInProgressFiber(runtime) == nullptr);
  assert(getWorkInProgressRootRenderLanes(runtime) == NoLanes);

  if (renderCurrent->alternate != nullptr) {
    delete renderCurrent->alternate;
    renderCurrent->alternate = nullptr;
  }
  delete renderChild;
  delete renderCurrent;

  // completeUnitOfWork traverses to siblings and eventually completes the root.
  FiberNode* parent = createFiber(WorkTag::HostRoot);
  FiberNode* childA = createFiber(WorkTag::FunctionComponent);
  FiberNode* childB = createFiber(WorkTag::HostComponent);
  parent->child = childA;
  childA->returnFiber = parent;
  childA->sibling = childB;
  childB->returnFiber = parent;

  setWorkInProgressRootExitStatus(runtime, RootExitStatus::InProgress);
  setWorkInProgressRootDidSkipSuspendedSiblings(runtime, false);
  setWorkInProgressFiber(runtime, childA);
  completeUnitOfWork(runtime, *childA);
  assert(getWorkInProgressFiber(runtime) == childB);
  assert(getWorkInProgressRootExitStatus(runtime) == RootExitStatus::InProgress);

  setWorkInProgressFiber(runtime, parent);
  parent->returnFiber = nullptr;
  parent->sibling = nullptr;
  completeUnitOfWork(runtime, *parent);
  assert(getWorkInProgressRootExitStatus(runtime) == RootExitStatus::Completed);
  assert(getWorkInProgressFiber(runtime) == nullptr);

  setWorkInProgressFiber(runtime, childA);
  setWorkInProgressRootExitStatus(runtime, RootExitStatus::InProgress);
  performUnitOfWork(runtime, *childA);
  assert(getWorkInProgressFiber(runtime) == childB);

  setWorkInProgressFiber(runtime, childA);
  workLoopSync(runtime);
  assert(getWorkInProgressFiber(runtime) == nullptr);

  // workLoopConcurrent should process limited slices; with trivial beginWork it finishes immediately.
  setWorkInProgressFiber(runtime, childA);
  workLoopConcurrent(runtime, true);
  assert(getWorkInProgressFiber(runtime) == nullptr);

  resetState(runtime);
  FiberRoot concurrentRoot{};
  FiberNode* concurrentCurrent = createFiber(WorkTag::HostRoot);
  FiberNode* concurrentChild = createFiber(WorkTag::FunctionComponent);
  concurrentCurrent->child = concurrentChild;
  concurrentChild->returnFiber = concurrentCurrent;
  concurrentRoot.current = concurrentCurrent;

  RootExitStatus concurrentStatus = renderRootConcurrent(runtime, concurrentRoot, DefaultLane);
  assert(concurrentStatus == RootExitStatus::Completed);
  assert(getWorkInProgressRoot(runtime) == nullptr);
  assert(getWorkInProgressFiber(runtime) == nullptr);
  assert(getWorkInProgressRootRenderLanes(runtime) == NoLanes);

  if (FiberNode* workInProgressRoot = concurrentCurrent->alternate) {
    workInProgressRoot->child = nullptr;
    workInProgressRoot->alternate = nullptr;
    delete workInProgressRoot;
    concurrentCurrent->alternate = nullptr;
  }

  concurrentCurrent->child = nullptr;
  concurrentChild->returnFiber = nullptr;
  delete concurrentChild;
  delete concurrentCurrent;

  // throwAndUnwindWorkLoop should mark skipped siblings and unwind to the shell.
  FiberRoot throwRoot{};
  FiberNode* throwParent = createFiber(WorkTag::HostRoot);
  FiberNode* throwChild = createFiber(WorkTag::FunctionComponent);
  throwChild->returnFiber = throwParent;
  throwChild->flags = static_cast<FiberFlags>(throwChild->flags | Incomplete);
  setWorkInProgressRoot(runtime, &throwRoot);
  setWorkInProgressRootRenderLanes(runtime, DefaultLane);
  setWorkInProgressRootIsPrerendering(runtime, false);
  setWorkInProgressRootDidSkipSuspendedSiblings(runtime, false);
  setWorkInProgressRootExitStatus(runtime, RootExitStatus::InProgress);
  setWorkInProgressFiber(runtime, throwChild);
  throwAndUnwindWorkLoop(
    runtime,
    throwRoot,
    *throwChild,
    nullptr,
    SuspendedReason::SuspendedOnData);
  assert(getWorkInProgressRootDidSkipSuspendedSiblings(runtime));
  assert(getWorkInProgressRootExitStatus(runtime) == RootExitStatus::SuspendedAtTheShell);
  assert(getWorkInProgressFiber(runtime) == nullptr);
  delete throwChild;
  delete throwParent;

  // unwindUnitOfWork marks parents incomplete and climbs to siblings.
  FiberNode* unwindParent = createFiber(WorkTag::HostRoot);
  FiberNode* unwindChild = createFiber(WorkTag::FunctionComponent);
  FiberNode* unwindSibling = createFiber(WorkTag::FunctionComponent);
  unwindParent->child = unwindChild;
  unwindChild->returnFiber = unwindParent;
  unwindChild->sibling = unwindSibling;
  setWorkInProgressFiber(runtime, unwindChild);
  unwindUnitOfWork(runtime, *unwindChild, false);
  assert(getWorkInProgressFiber(runtime) == unwindSibling);
  assert((unwindParent->flags & Incomplete) == Incomplete);
  assert(unwindParent->subtreeFlags == NoFlags);

  FiberNode* unwoundRoot = createFiber(WorkTag::HostRoot);
  setWorkInProgressRootExitStatus(runtime, RootExitStatus::InProgress);
  setWorkInProgressFiber(runtime, unwoundRoot);
  unwindUnitOfWork(runtime, *unwoundRoot, true);
  assert(getWorkInProgressRootExitStatus(runtime) == RootExitStatus::SuspendedAtTheShell);
  assert(getWorkInProgressFiber(runtime) == nullptr);

  delete parent;
  delete childA;
  delete childB;
  delete unwindParent;
  delete unwindChild;
  delete unwindSibling;
  delete unwoundRoot;

  resetState(runtime);
  return true;
}

} // namespace react::test
