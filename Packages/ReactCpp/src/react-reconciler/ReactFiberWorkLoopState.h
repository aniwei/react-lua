#pragma once

#include "react-reconciler/ReactFiberLane.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace react {

class FiberNode;
struct FiberRoot;
struct Transition;

using ExecutionContext = std::uint8_t;

inline constexpr ExecutionContext NoContext = 0b000;
inline constexpr ExecutionContext BatchedContext = 0b001;
inline constexpr ExecutionContext RenderContext = 0b010;
inline constexpr ExecutionContext CommitContext = 0b100;

inline constexpr double fallbackThrottleMs = 300.0;
inline constexpr double renderTimeoutMs = 500.0;
inline constexpr std::uint32_t nestedUpdateLimit = 50;
inline constexpr std::uint32_t nestedPassiveUpdateLimit = 50;

enum class PendingEffectsStatus : std::uint8_t {
  None = 0,
  Mutation = 1,
  Layout = 2,
  AfterMutation = 3,
  SpawnedWork = 4,
  Passive = 5,
  GestureMutation = 6,
  GestureAnimation = 7,
};

enum class SuspendedCommitReason : std::uint8_t {
  ImmediateCommit = 0,
  SuspendedCommit = 1,
  ThrottledCommit = 2,
};

enum class RootExitStatus : std::uint8_t {
  InProgress = 0,
  FatalErrored = 1,
  Errored = 2,
  Suspended = 3,
  SuspendedWithDelay = 4,
  Completed = 5,
  SuspendedAtTheShell = 6,
};

enum class SuspendedReason : std::uint8_t {
  NotSuspended = 0,
  SuspendedOnError = 1,
  SuspendedOnData = 2,
  SuspendedOnImmediate = 3,
  SuspendedOnInstance = 4,
  SuspendedOnInstanceAndReadyToContinue = 5,
  SuspendedOnDeprecatedThrowPromise = 6,
  SuspendedAndReadyToContinue = 7,
  SuspendedOnHydration = 8,
  SuspendedOnAction = 9,
};

struct WorkLoopState {
  ExecutionContext executionContext{NoContext};
  FiberRoot* workInProgressRoot{nullptr};
  FiberNode* workInProgressFiber{nullptr};
  Lanes workInProgressRootRenderLanes{NoLanes};
  Lanes entangledRenderLanes{NoLanes};
  SuspendedReason suspendedReason{SuspendedReason::NotSuspended};
  void* thrownValue{nullptr};
  bool didSkipSuspendedSiblings{false};
  bool isPrerendering{false};
  bool didAttachPingListener{false};
  RootExitStatus exitStatus{RootExitStatus::InProgress};
  Lanes skippedLanes{NoLanes};
  Lanes interleavedUpdatedLanes{NoLanes};
  Lanes renderPhaseUpdatedLanes{NoLanes};
  Lanes pingedLanes{NoLanes};
  Lane deferredLane{NoLane};
  Lanes suspendedRetryLanes{NoLanes};
  std::vector<void*> concurrentErrors{};
  std::vector<void*> recoverableErrors{};
  bool didIncludeRecursiveRenderUpdate{false};
  bool didIncludeCommitPhaseUpdate{false};
  double globalMostRecentFallbackTime{0.0};
  double workInProgressRootRenderTargetTime{std::numeric_limits<double>::infinity()};
  std::vector<const Transition*> workInProgressTransitions{};
  void* workInProgressUpdateTask{nullptr};
  void* currentPendingTransitionCallbacks{nullptr};
  double currentEndTime{0.0};
  double currentNewestExplicitSuspenseTime{0.0};
  PendingEffectsStatus pendingEffectsStatus{PendingEffectsStatus::None};
  FiberRoot* pendingEffectsRoot{nullptr};
  FiberNode* pendingFinishedWork{nullptr};
  Lanes pendingEffectsLanes{NoLanes};
  Lanes pendingEffectsRemainingLanes{NoLanes};
  double pendingEffectsRenderEndTime{-0.0};
  std::vector<const Transition*> pendingPassiveTransitions{};
  std::vector<void*> pendingRecoverableErrors{};
  void* pendingViewTransition{nullptr};
  std::vector<void*> pendingViewTransitionEvents{};
  void* pendingTransitionTypes{nullptr};
  bool pendingDidIncludeRenderPhaseUpdate{false};
  SuspendedCommitReason pendingSuspendedCommitReason{SuspendedCommitReason::ImmediateCommit};
  std::uint32_t nestedUpdateCount{0};
  FiberRoot* rootWithNestedUpdates{nullptr};
  bool isFlushingPassiveEffects{false};
  bool didScheduleUpdateDuringPassiveEffects{false};
  std::uint32_t nestedPassiveUpdateCount{0};
  FiberRoot* rootWithPassiveNestedUpdates{nullptr};
  bool isRunningInsertionEffect{false};
};

} // namespace react
