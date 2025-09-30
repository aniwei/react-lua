#pragma once

#include "jsi/jsi.h"
#include "react-reconciler/ReactFiberFlags.h"
#include "react-reconciler/ReactFiberLane.h"
#include "react-reconciler/ReactWorkTags.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace jsi = facebook::jsi;

namespace react {

struct UpdateQueue;

struct FiberDependencies {
  Lanes lanes{0};
  jsi::Value firstContext;
};

class FiberNode {
public:
  WorkTag tag;
  jsi::Value elementType;
  jsi::Value type;
  jsi::Value key;
  jsi::Value ref;

  void* stateNode;

    std::shared_ptr<FiberNode> returnFiber;
    std::shared_ptr<FiberNode> child;
    std::shared_ptr<FiberNode> sibling;
  uint32_t index{0};

  jsi::Value pendingProps;
  jsi::Value memoizedProps;
  jsi::Value pendingState;
  jsi::Value memoizedState;
  std::shared_ptr<UpdateQueue> updateQueue;
  jsi::Value updatePayload;
  std::unique_ptr<FiberDependencies> dependencies;

  uint32_t mode{0};
  Lanes lanes{0};
  Lanes childLanes{0};

  FiberFlags flags{NoFlags};
  FiberFlags subtreeFlags{NoFlags};
    std::vector<std::shared_ptr<FiberNode>> deletions;

    std::shared_ptr<FiberNode> alternate;

  double actualDuration{0.0};
  double actualStartTime{-1.0};
  double selfBaseDuration{0.0};
  double treeBaseDuration{0.0};

  bool isHydrating{false};

  FiberNode(WorkTag tag, jsi::Value pendingProps, jsi::Value key, uint32_t mode = 0, Lanes lanes = 0);
};

} // namespace react
