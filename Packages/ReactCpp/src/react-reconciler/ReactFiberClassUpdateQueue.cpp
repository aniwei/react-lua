#include "react-reconciler/ReactFiberClassUpdateQueue.h"

#include <utility>

namespace react {
namespace {

ClassUpdateQueue* cloneClassUpdateQueue(const ClassUpdateQueue& source) {
  auto* queue = new ClassUpdateQueue();
  queue->baseState = source.baseState;

  ClassUpdate* current = source.firstBaseUpdate;
  ClassUpdate* previousClone = nullptr;
  while (current != nullptr) {
    auto clone = std::make_unique<ClassUpdate>();
    clone->lane = current->lane;
    clone->tag = current->tag;
    clone->payload = current->payload;
    clone->callback = current->callback;
    clone->next = nullptr;

    ClassUpdate* clonePtr = clone.get();
    queue->ownedUpdates.push_back(std::move(clone));

    if (previousClone == nullptr) {
      queue->firstBaseUpdate = clonePtr;
    } else {
      previousClone->next = clonePtr;
    }
    previousClone = clonePtr;

    current = current->next;
  }

  queue->lastBaseUpdate = previousClone;
  return queue;
}

ClassUpdateQueue* createClassUpdateQueue(FiberNode& fiber) {
  auto* queue = new ClassUpdateQueue();
  queue->baseState = fiber.memoizedState;
  queue->firstBaseUpdate = nullptr;
  queue->lastBaseUpdate = nullptr;
  return queue;
}

} // namespace

ClassUpdateQueue& ensureClassUpdateQueue(FiberNode& fiber) {
  auto* queue = static_cast<ClassUpdateQueue*>(fiber.updateQueue);
  if (queue != nullptr) {
    return *queue;
  }

  if (FiberNode* const current = fiber.alternate) {
    if (auto* const currentQueue = static_cast<ClassUpdateQueue*>(current->updateQueue)) {
      queue = cloneClassUpdateQueue(*currentQueue);
      fiber.updateQueue = queue;
      return *queue;
    }
  }

  queue = createClassUpdateQueue(fiber);
  fiber.updateQueue = queue;
  return *queue;
}

std::unique_ptr<ClassUpdate> createRootErrorClassUpdate(
    FiberRoot& root,
    const CapturedValue& errorInfo,
    Lane lane) {
  auto update = std::make_unique<ClassUpdate>();
  update->lane = lane;
  update->tag = ClassUpdateTag::CaptureUpdate;
  update->payload = nullptr;
  update->callback = [&root, captured = errorInfo]() mutable {
    logUncaughtError(root, captured);
  };
  update->next = nullptr;
  return update;
}

std::unique_ptr<ClassUpdate> createClassErrorUpdate(Lane lane) {
  auto update = std::make_unique<ClassUpdate>();
  update->lane = lane;
  update->tag = ClassUpdateTag::CaptureUpdate;
  update->payload = nullptr;
  update->callback = nullptr;
  update->next = nullptr;
  return update;
}

void initializeClassErrorUpdate(
    ClassUpdate& update,
    FiberRoot& root,
    FiberNode& fiber,
    const CapturedValue& errorInfo) {
  update.payload = errorInfo.value;

  void* const instance = fiber.stateNode;
    update.callback = [instance, &root, &fiber, source = errorInfo.source, value = errorInfo.value, stack = errorInfo.stack]() {
    if (instance != nullptr) {
      markLegacyErrorBoundaryAsFailed(instance);
    }

      CapturedValue wrapped{value, source, stack};
    logCaughtError(root, fiber, wrapped);
  };
}

void enqueueCapturedClassUpdate(FiberNode& fiber, std::unique_ptr<ClassUpdate> update) {
  ClassUpdateQueue& queue = ensureClassUpdateQueue(fiber);
  ClassUpdate* const updatePtr = update.get();
  updatePtr->next = nullptr;
  queue.ownedUpdates.push_back(std::move(update));

  if (queue.lastBaseUpdate == nullptr) {
    queue.firstBaseUpdate = updatePtr;
    queue.lastBaseUpdate = updatePtr;
  } else {
    queue.lastBaseUpdate->next = updatePtr;
    queue.lastBaseUpdate = updatePtr;
  }
}

void pushClassUpdate(FiberNode& fiber, std::unique_ptr<ClassUpdate> update) {
  ClassUpdateQueue& queue = ensureClassUpdateQueue(fiber);
  ClassUpdate* const updatePtr = update.get();
  updatePtr->next = nullptr;
  queue.ownedUpdates.push_back(std::move(update));

  if (queue.lastBaseUpdate == nullptr) {
    queue.firstBaseUpdate = updatePtr;
    queue.lastBaseUpdate = updatePtr;
  } else {
    queue.lastBaseUpdate->next = updatePtr;
    queue.lastBaseUpdate = updatePtr;
  }
}

} // namespace react
