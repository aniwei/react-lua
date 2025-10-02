add_library(react_cpp_src
    react-dom/client/ReactDOMComponent.cpp
    react-dom/client/ReactDOMDiffProperties.cpp
    react-dom/client/ReactDOMInstance.cpp
    react-reconciler/ReactFiberConcurrentUpdates.cpp
    react-reconciler/ReactCapturedValue.cpp
    react-reconciler/ReactFiber.cpp
    react-reconciler/ReactFiberAsyncAction.cpp
    react-reconciler/ReactFiberErrorLogger.cpp
    react-reconciler/ReactFiberHiddenContext.cpp
    react-reconciler/ReactFiberClassUpdateQueue.cpp
    react-reconciler/ReactFiberStack.cpp
    react-reconciler/ReactFiberSuspenseContext.cpp
    react-reconciler/ReactFiberThenable.cpp
    react-reconciler/ReactFiberThrow.cpp
    react-reconciler/ReactFiberRootScheduler.cpp
    react-reconciler/ReactWakeable.cpp
    react-reconciler/ReactUpdateQueue.cpp
    react-reconciler/ReactFiberWorkLoop.cpp
    runtime/ReactRuntime.cpp
    shared/ReactOwnerStackReset.cpp
    shared/ReactSharedInternals.cpp
    shared/ReactSymbols.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../jsi/jsi/jsi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../jsi/jsi/jsilib-posix.cpp
)

# This would be where you link against JSI, etc.
# target_link_libraries(react_cpp_src PRIVATE jsi)

target_include_directories(react_cpp_src PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../jsi
)
