#include "react-reconciler/ReactFiber.h"
#include "react-reconciler/ReactFiberFlags.h"
#include "react-reconciler/ReactFiberLane.h"
#include "react-reconciler/ReactRootTags.h"

#include <cassert>
#include <memory>

namespace react::test {

namespace {

void clearAlternateLinks(FiberNode* a, FiberNode* b) {
  if (a != nullptr) {
    a->alternate = nullptr;
  }
  if (b != nullptr) {
    b->alternate = nullptr;
  }
}

} // namespace

bool runReactFiberRuntimeTests() {
  {
    FiberNode* hostRoot = createHostRootFiber(RootTag::ConcurrentRoot, true);
    assert(hostRoot != nullptr);
    assert(hostRoot->tag == WorkTag::HostRoot);
    assert((hostRoot->mode & ConcurrentMode) == ConcurrentMode);
    assert((hostRoot->mode & StrictLegacyMode) == StrictLegacyMode);
    assert((hostRoot->mode & StrictEffectsMode) == StrictEffectsMode);
    delete hostRoot;
  }

  {
    FiberNode* fiber = createFiber(WorkTag::FunctionComponent, reinterpret_cast<void*>(0x1), "no-alternate", ConcurrentMode);
    auto dependencies = std::make_unique<FiberNode::Dependencies>();
    dependencies->lanes = DefaultLane;
    dependencies->firstContext = reinterpret_cast<void*>(0x2);
    fiber->dependencies = std::move(dependencies);

    resetWorkInProgress(fiber, DefaultLane);

    assert(fiber->dependencies == nullptr);
    delete fiber;
  }

  {
    FiberNode* current = createFiber(WorkTag::FunctionComponent, reinterpret_cast<void*>(0x1), "key", ConcurrentMode);
    current->memoizedProps = reinterpret_cast<void*>(0x2);
  current->memoizedState = reinterpret_cast<void*>(0x3);
  current->updateQueue = reinterpret_cast<void*>(0x4);
  auto dependencies = std::make_unique<FiberNode::Dependencies>();
  dependencies->lanes = DefaultLane;
  dependencies->firstContext = reinterpret_cast<void*>(0x5);
  current->dependencies = std::move(dependencies);
    current->lanes = DefaultLane;
    current->childLanes = DefaultLane;
    current->flags = LayoutStatic;
    current->ref = reinterpret_cast<void*>(0x6);
    current->refCleanup = reinterpret_cast<void*>(0x7);

    FiberNode* work = createWorkInProgress(current, reinterpret_cast<void*>(0x8));
    assert(work != nullptr);
    assert(work->pendingProps == reinterpret_cast<void*>(0x8));
    assert(work->ref == current->ref);
    assert(work->refCleanup == current->refCleanup);
  assert(work->memoizedProps == current->memoizedProps);
  assert(work->dependencies != nullptr);
  assert(work->dependencies->lanes == DefaultLane);
  assert(work->dependencies->firstContext == reinterpret_cast<void*>(0x5));

    work->flags |= Update;
    work->memoizedProps = reinterpret_cast<void*>(0x9);
    work->memoizedState = reinterpret_cast<void*>(0xA);
    work->updateQueue = reinterpret_cast<void*>(0xB);
  work->dependencies->lanes = SyncLane;
  work->dependencies->firstContext = reinterpret_cast<void*>(0xC);
    work->deletions.push_back(current);

    resetWorkInProgress(work, DefaultLane);

    assert(work->pendingProps == reinterpret_cast<void*>(0x8));
    assert(work->memoizedProps == current->memoizedProps);
  assert(work->memoizedState == current->memoizedState);
  assert(work->updateQueue == current->updateQueue);
  assert(work->dependencies != nullptr);
  assert(work->dependencies->lanes == DefaultLane);
  assert(work->dependencies->firstContext == reinterpret_cast<void*>(0x5));
    assert(work->child == current->child);
    assert(work->deletions.empty());
    assert((work->flags & Update) == 0);
    assert((work->flags & LayoutStatic) == LayoutStatic);

    clearAlternateLinks(current, work);
    delete work;
    delete current;
  }

  return true;
}

} // namespace react::test
