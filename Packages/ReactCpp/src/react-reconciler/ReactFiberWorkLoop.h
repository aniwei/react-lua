#pragma once

// Backwards compatibility shim. The reconciler header now simply forwards to the
// canonical runtime implementation to avoid duplicated definitions.
#include "../runtime/ReactFiberWorkLoop.h"
