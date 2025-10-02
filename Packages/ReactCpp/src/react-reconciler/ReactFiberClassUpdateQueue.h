#pragma once

#include "react-reconciler/ReactFiber.h"
#include "react-reconciler/ReactFiberLane.h"
#include "react-reconciler/ReactFiberErrorLogger.h"
#include "react-reconciler/ReactCapturedValue.h"

#include <functional>
#include <memory>
#include <vector>

namespace react {

struct FiberRoot;

bool isAlreadyFailedLegacyErrorBoundary(void* instance);
void markLegacyErrorBoundaryAsFailed(void* instance);

enum class ClassUpdateTag : std::uint8_t {
  UpdateState = 0,
  ReplaceState = 1,
  ForceUpdate = 2,
  CaptureUpdate = 3,
};

struct ClassUpdate {
  Lane lane{NoLane};
  ClassUpdateTag tag{ClassUpdateTag::UpdateState};
  void* payload{nullptr};
  std::function<void()> callback{};
  ClassUpdate* next{nullptr};
};

struct ClassUpdateQueue {
  void* baseState{nullptr};
  ClassUpdate* firstBaseUpdate{nullptr};
  ClassUpdate* lastBaseUpdate{nullptr};
  std::vector<std::unique_ptr<ClassUpdate>> ownedUpdates{};
};

ClassUpdateQueue& ensureClassUpdateQueue(FiberNode& fiber);
std::unique_ptr<ClassUpdate> createRootErrorClassUpdate(
    FiberRoot& root,
    const CapturedValue& errorInfo,
    Lane lane);
std::unique_ptr<ClassUpdate> createClassErrorUpdate(Lane lane);
void initializeClassErrorUpdate(
    ClassUpdate& update,
    FiberRoot& root,
    FiberNode& fiber,
    const CapturedValue& errorInfo);
void enqueueCapturedClassUpdate(FiberNode& fiber, std::unique_ptr<ClassUpdate> update);
void pushClassUpdate(FiberNode& fiber, std::unique_ptr<ClassUpdate> update);

} // namespace react
