#pragma once

#include "react-reconciler/ReactFiberStack.h"

#include <cstdint>

namespace react {

class FiberNode;

using SuspenseContext = std::uint8_t;
using SubtreeSuspenseContext = std::uint8_t;
using ShallowSuspenseContext = std::uint8_t;

inline constexpr SuspenseContext DefaultSuspenseContext = 0b00;
inline constexpr SuspenseContext SubtreeSuspenseContextMask = 0b01;
inline constexpr ShallowSuspenseContext ForceSuspenseFallback = 0b10;

FiberNode* getShellBoundary();
FiberNode* getSuspenseHandler();

void pushPrimaryTreeSuspenseHandler(FiberNode& handler);
void pushFallbackTreeSuspenseHandler(FiberNode& fiber);
void pushDehydratedActivitySuspenseHandler(FiberNode& fiber);
void pushOffscreenSuspenseHandler(FiberNode& fiber);
void reuseSuspenseHandlerOnStack(FiberNode& fiber);
void popSuspenseHandler(FiberNode& fiber);

bool hasSuspenseListContext(SuspenseContext parentContext, SuspenseContext flag);
SuspenseContext setDefaultShallowSuspenseListContext(SuspenseContext parentContext);
SuspenseContext setShallowSuspenseListContext(
    SuspenseContext parentContext,
    ShallowSuspenseContext shallowContext);
void pushSuspenseListContext(FiberNode& fiber, SuspenseContext newContext);
void popSuspenseListContext(FiberNode& fiber);

SuspenseContext getCurrentSuspenseContext();

} // namespace react
