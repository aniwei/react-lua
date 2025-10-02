#pragma once

#include "react-reconciler/ReactWakeable.h"

namespace react {

Wakeable& noopSuspenseyCommitThenable();
bool isNoopSuspenseyCommitThenable(const Wakeable* wakeable);
bool isNoopSuspenseyCommitThenable(const void* value);

} // namespace react
