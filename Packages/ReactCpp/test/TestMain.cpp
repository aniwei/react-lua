#include <cstdlib>

namespace react::test {
bool runUpdateQueueTests();
bool runReactFiberLaneRuntimeTests();
bool runReactFiberConcurrentUpdatesRuntimeTests();
}

int main() {
    bool allPassed = true;
    allPassed &= react::test::runUpdateQueueTests();
    allPassed &= react::test::runReactFiberLaneRuntimeTests();
    allPassed &= react::test::runReactFiberConcurrentUpdatesRuntimeTests();
    return allPassed ? EXIT_SUCCESS : EXIT_FAILURE;
}
