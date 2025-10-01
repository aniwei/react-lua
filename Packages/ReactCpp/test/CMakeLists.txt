add_executable(react_cpp_runtime_tests
    TestMain.cpp
    ReactFiberLaneTests.cpp
    ReactFiberLaneRuntimeTests.cpp
    ReactFiberConcurrentUpdatesRuntimeTests.cpp
    ReactFiberRuntimeTests.cpp
    ReactFiberWorkLoopStateTests.cpp
    ReactSharedConstantsTests.cpp
    UpdateQueueTests.cpp
)

set_target_properties(react_cpp_runtime_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

target_link_libraries(react_cpp_runtime_tests PRIVATE react_cpp_src)

target_include_directories(react_cpp_runtime_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
