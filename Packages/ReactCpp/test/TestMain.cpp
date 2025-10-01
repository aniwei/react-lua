#include <cstdlib>

namespace react::test {
bool runUpdateQueueTests();
bool runReactFiberLaneRuntimeTests();
bool runReactFiberConcurrentUpdatesRuntimeTests();
bool runReactFiberRuntimeTests();
bool runReactFiberWorkLoopStateTests();
}

int main() {
    bool allPassed = true;
    allPassed &= react::test::runUpdateQueueTests();
    allPassed &= react::test::runReactFiberLaneRuntimeTests();
    allPassed &= react::test::runReactFiberConcurrentUpdatesRuntimeTests();
    allPassed &= react::test::runReactFiberRuntimeTests();
    allPassed &= react::test::runReactFiberWorkLoopStateTests();
    return allPassed ? EXIT_SUCCESS : EXIT_FAILURE;
}
