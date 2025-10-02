#include "react-reconciler/ReactFiberSuspenseContext.h"

#include "react-reconciler/ReactFiber.h"
#include "react-reconciler/ReactFiberHiddenContext.h"
#include "react-reconciler/ReactFiberStack.h"
#include "react-reconciler/ReactFiberSuspenseComponent.h"
#include "react-reconciler/ReactWorkTags.h"
#include "shared/ReactFeatureFlags.h"

namespace react {
namespace {

StackCursor<FiberNode*> gSuspenseHandlerStackCursor = createCursor<FiberNode*>(nullptr);
StackCursor<SuspenseContext> gSuspenseStackCursor = createCursor<SuspenseContext>(DefaultSuspenseContext);
FiberNode* gShellBoundary = nullptr;

} // namespace

FiberNode* getShellBoundary() {
  return gShellBoundary;
}

FiberNode* getSuspenseHandler() {
  return gSuspenseHandlerStackCursor.current;
}

SuspenseContext getCurrentSuspenseContext() {
  return gSuspenseStackCursor.current;
}

void pushSuspenseListContext(FiberNode& fiber, SuspenseContext newContext) {
  push(gSuspenseStackCursor, newContext, &fiber);
}

void popSuspenseListContext(FiberNode& fiber) {
  pop(gSuspenseStackCursor, &fiber);
}

bool hasSuspenseListContext(SuspenseContext parentContext, SuspenseContext flag) {
  return (parentContext & flag) != 0;
}

SuspenseContext setDefaultShallowSuspenseListContext(SuspenseContext parentContext) {
  return parentContext & SubtreeSuspenseContextMask;
}

SuspenseContext setShallowSuspenseListContext(
    SuspenseContext parentContext,
    ShallowSuspenseContext shallowContext) {
  return (parentContext & SubtreeSuspenseContextMask) | shallowContext;
}

void pushPrimaryTreeSuspenseHandler(FiberNode& handler) {
  FiberNode* const current = handler.alternate;
  [[maybe_unused]] auto* const props = static_cast<SuspenseProps*>(handler.pendingProps);

  pushSuspenseListContext(handler, setDefaultShallowSuspenseListContext(gSuspenseStackCursor.current));

  if constexpr (enableSuspenseAvoidThisFallback) {
    const bool avoidFallback = props != nullptr && props->unstable_avoidThisFallback;
    const bool isHidden = current == nullptr || isCurrentTreeHidden();
    if (avoidFallback && isHidden) {
      if (gShellBoundary == nullptr) {
        push(gSuspenseHandlerStackCursor, &handler, &handler);
        return;
      }
      FiberNode* const handlerOnStack = gSuspenseHandlerStackCursor.current;
      push(gSuspenseHandlerStackCursor, handlerOnStack, &handler);
      return;
    }
  }

  push(gSuspenseHandlerStackCursor, &handler, &handler);
  if (gShellBoundary == nullptr) {
    if (current == nullptr || isCurrentTreeHidden()) {
      gShellBoundary = &handler;
    } else {
      const auto* const prevState = current != nullptr
          ? static_cast<SuspenseState*>(current->memoizedState)
          : nullptr;
      if (prevState != nullptr) {
        gShellBoundary = &handler;
      }
    }
  }
}

void pushFallbackTreeSuspenseHandler(FiberNode& fiber) {
  reuseSuspenseHandlerOnStack(fiber);
}

void pushDehydratedActivitySuspenseHandler(FiberNode& fiber) {
  pushSuspenseListContext(fiber, gSuspenseStackCursor.current);
  push(gSuspenseHandlerStackCursor, &fiber, &fiber);
  if (gShellBoundary == nullptr) {
    gShellBoundary = &fiber;
  }
}

void pushOffscreenSuspenseHandler(FiberNode& fiber) {
  if (fiber.tag == WorkTag::OffscreenComponent) {
    pushSuspenseListContext(fiber, gSuspenseStackCursor.current);
    push(gSuspenseHandlerStackCursor, &fiber, &fiber);
    if (gShellBoundary == nullptr) {
      gShellBoundary = &fiber;
    }
  } else {
    reuseSuspenseHandlerOnStack(fiber);
  }
}

void reuseSuspenseHandlerOnStack(FiberNode& fiber) {
  pushSuspenseListContext(fiber, gSuspenseStackCursor.current);
  FiberNode* const handlerOnStack = gSuspenseHandlerStackCursor.current;
  push(gSuspenseHandlerStackCursor, handlerOnStack, &fiber);
}

void popSuspenseHandler(FiberNode& fiber) {
  pop(gSuspenseHandlerStackCursor, &fiber);
  if (gShellBoundary == &fiber) {
    gShellBoundary = nullptr;
  }
  popSuspenseListContext(fiber);
}

} // namespace react
