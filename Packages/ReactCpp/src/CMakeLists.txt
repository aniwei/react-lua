add_library(react_cpp_src
    host/HostInstance.cpp
    host/SimpleHostInstance.cpp
    runtime/ReactRuntime.cpp
    reconciler/FiberNode.cpp
    reconciler/UpdateQueue.cpp
    runtime/ReactWasmBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../jsi/jsi/jsi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../jsi/jsi/jsilib-posix.cpp
)

# This would be where you link against JSI, etc.
# target_link_libraries(react_cpp_src PRIVATE jsi)

target_include_directories(react_cpp_src PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../jsi
)
