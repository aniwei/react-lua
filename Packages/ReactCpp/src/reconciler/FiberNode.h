#pragma once

#include "jsi/jsi.h"
#include <memory>

namespace react {

// Forward declarations
#pragma once

#include "jsi/jsi.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace jsi = facebook::jsi;

namespace react {

class HostInstance;
struct UpdateQueue;

using Lane = uint32_t;
using Lanes = uint32_t;

enum class WorkTag {
  IndeterminateComponent,
  FunctionComponent,
  ClassComponent,
  HostRoot,
  HostComponent,
  HostText,
  HostPortal,
  ForwardRef,
  Fragment,
  Mode,
  ContextProvider,
  ContextConsumer,
  SuspenseComponent,
  SuspenseListComponent,
  MemoComponent,
  SimpleMemoComponent,
  LazyComponent,
  IncompleteClassComponent,
  // ... extend as needed
};

enum class FiberFlags : uint32_t {
  NoFlags = 0,
  Placement = 1 << 0,
  Update = 1 << 1,
  Deletion = 1 << 2,
  ChildDeletion = 1 << 3,
  Ref = 1 << 4,
  Snapshot = 1 << 5,
  Passive = 1 << 6,
  Hydrating = 1 << 7,
  Visibility = 1 << 8,
  StoreConsistency = 1 << 9,
};

inline FiberFlags operator|(FiberFlags a, FiberFlags b) {
  return static_cast<FiberFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline FiberFlags& operator|=(FiberFlags& a, FiberFlags b) {
  a = a | b;
  return a;
}

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

  std::shared_ptr<HostInstance> stateNode;

  std::shared_ptr<FiberNode> returnFiber;
  std::shared_ptr<FiberNode> child;
  std::shared_ptr<FiberNode> sibling;
  uint32_t index{0};

  jsi::Value pendingProps;
  jsi::Value memoizedProps;
  jsi::Value pendingState;
  jsi::Value memoizedState;
  std::shared_ptr<UpdateQueue> updateQueue;
  std::unique_ptr<FiberDependencies> dependencies;

  uint32_t mode{0};
  Lanes lanes{0};
  Lanes childLanes{0};

  FiberFlags flags{FiberFlags::NoFlags};
  FiberFlags subtreeFlags{FiberFlags::NoFlags};
  std::vector<std::shared_ptr<FiberNode>> deletions;

  std::shared_ptr<FiberNode> alternate;

  double actualDuration{0.0};
  double actualStartTime{-1.0};
  double selfBaseDuration{0.0};
  double treeBaseDuration{0.0};

  bool isHydrating{false};

  FiberNode(WorkTag tag, jsi::Value pendingProps, jsi::Value key, uint32_t mode = 0, Lanes lanes = 0);
};
    // The associated host instance (e.g., a DOM node proxy).
