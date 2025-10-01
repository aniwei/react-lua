#include "react-reconciler/ReactFiber.h"

#include "react-reconciler/ReactFiberFlags.h"
#include "react-reconciler/ReactFiberLane.h"
#include "react-reconciler/ReactRootTags.h"
#include "shared/ReactFeatureFlags.h"

#include <utility>

namespace react {

namespace {

constexpr bool kIsDevToolsPresent = false;

void initializeProfilerDurations(FiberNode& fiber) {
  if (enableProfilerTimer) {
    fiber.actualDuration = -0.0;
    fiber.actualStartTime = -1.0;
    fiber.selfBaseDuration = -0.0;
    fiber.treeBaseDuration = -0.0;
  } else {
    fiber.actualDuration = 0.0;
    fiber.actualStartTime = 0.0;
    fiber.selfBaseDuration = 0.0;
    fiber.treeBaseDuration = 0.0;
  }
}

std::unique_ptr<FiberNode::Dependencies> cloneDependencies(
    const FiberNode::Dependencies* source) {
  if (source == nullptr) {
    return nullptr;
  }

  auto clone = std::make_unique<FiberNode::Dependencies>();
  clone->lanes = source->lanes;
  clone->firstContext = source->firstContext;
  return clone;
}

} // namespace

FiberNode* createFiber(
    WorkTag tag,
    void* pendingProps,
    std::string key,
    TypeOfMode mode) {
  auto* fiber = new FiberNode();
  fiber->tag = tag;
  fiber->key = std::move(key);
  fiber->elementType = nullptr;
  fiber->type = nullptr;
  fiber->stateNode = nullptr;

  fiber->returnFiber = nullptr;
  fiber->child = nullptr;
  fiber->sibling = nullptr;
  fiber->index = 0;

  fiber->ref = nullptr;
  fiber->refCleanup = nullptr;

  fiber->pendingProps = pendingProps;
  fiber->memoizedProps = nullptr;
  fiber->updateQueue = nullptr;
  fiber->memoizedState = nullptr;
  fiber->dependencies.reset();

  fiber->mode = mode;

  fiber->flags = NoFlags;
  fiber->subtreeFlags = NoFlags;
  fiber->deletions.clear();

  fiber->lanes = NoLanes;
  fiber->childLanes = NoLanes;

  fiber->alternate = nullptr;

  initializeProfilerDurations(*fiber);

  return fiber;
}

FiberNode* createWorkInProgress(FiberNode* current, void* pendingProps) {
  if (current == nullptr) {
    return nullptr;
  }

  FiberNode* workInProgress = current->alternate;
  if (workInProgress == nullptr) {
    workInProgress = createFiber(current->tag, pendingProps, current->key, current->mode);
    workInProgress->elementType = current->elementType;
    workInProgress->type = current->type;
    workInProgress->stateNode = current->stateNode;

    workInProgress->alternate = current;
    current->alternate = workInProgress;
  } else {
    workInProgress->pendingProps = pendingProps;
    workInProgress->type = current->type;

    workInProgress->flags = NoFlags;
    workInProgress->subtreeFlags = NoFlags;
    workInProgress->deletions.clear();

    if (enableProfilerTimer) {
      workInProgress->actualDuration = -0.0;
      workInProgress->actualStartTime = -1.0;
    }
  }

  workInProgress->flags = static_cast<FiberFlags>(current->flags & StaticMask);
  workInProgress->childLanes = current->childLanes;
  workInProgress->lanes = current->lanes;

  workInProgress->child = current->child;
  workInProgress->memoizedProps = current->memoizedProps;
  workInProgress->memoizedState = current->memoizedState;
  workInProgress->updateQueue = current->updateQueue;
  workInProgress->dependencies = cloneDependencies(current->dependencies.get());

  workInProgress->sibling = current->sibling;
  workInProgress->index = current->index;
  workInProgress->ref = current->ref;
  workInProgress->refCleanup = current->refCleanup;

  if (enableProfilerTimer) {
    workInProgress->selfBaseDuration = current->selfBaseDuration;
    workInProgress->treeBaseDuration = current->treeBaseDuration;
  }

  return workInProgress;
}

FiberNode* resetWorkInProgress(FiberNode* workInProgress, Lanes renderLanes) {
  if (workInProgress == nullptr) {
    return nullptr;
  }

  workInProgress->flags &= static_cast<FiberFlags>(StaticMask | Placement);

  FiberNode* current = workInProgress->alternate;
  if (current == nullptr) {
    workInProgress->childLanes = NoLanes;
    workInProgress->lanes = renderLanes;

    workInProgress->child = nullptr;
    workInProgress->subtreeFlags = NoFlags;
    workInProgress->deletions.clear();
    workInProgress->memoizedProps = nullptr;
    workInProgress->memoizedState = nullptr;
    workInProgress->updateQueue = nullptr;
    workInProgress->dependencies.reset();
    workInProgress->stateNode = nullptr;

    if (enableProfilerTimer) {
      workInProgress->selfBaseDuration = 0.0;
      workInProgress->treeBaseDuration = 0.0;
    }
  } else {
    workInProgress->childLanes = current->childLanes;
    workInProgress->lanes = current->lanes;

    workInProgress->child = current->child;
    workInProgress->subtreeFlags = NoFlags;
    workInProgress->deletions.clear();
    workInProgress->memoizedProps = current->memoizedProps;
    workInProgress->memoizedState = current->memoizedState;
    workInProgress->updateQueue = current->updateQueue;
    workInProgress->type = current->type;
    workInProgress->dependencies = cloneDependencies(current->dependencies.get());

    if (enableProfilerTimer) {
      workInProgress->selfBaseDuration = current->selfBaseDuration;
      workInProgress->treeBaseDuration = current->treeBaseDuration;
    }
  }

  return workInProgress;
}

FiberNode* createHostRootFiber(RootTag tag, bool isStrictMode) {
  TypeOfMode mode = NoMode;

  if (disableLegacyMode || tag == RootTag::ConcurrentRoot) {
    mode = ConcurrentMode;
    if (isStrictMode) {
      mode |= StrictLegacyMode | StrictEffectsMode;
    }
  }

  if (enableProfilerTimer && kIsDevToolsPresent) {
    mode |= ProfileMode;
  }

  return createFiber(WorkTag::HostRoot, nullptr, std::string{}, mode);
}

} // namespace react
