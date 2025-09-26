add_library(react_cpp_src
    host/HostInstance.cpp
    runtime/ReactRuntime.cpp
    runtime/ReactWasmBridge.cpp
    reconciler/FiberNode.cpp
    reconciler/UpdateQueue.cpp
)

# This would be where you link against JSI, etc.
# target_link_libraries(react_cpp_src PRIVATE jsi)
