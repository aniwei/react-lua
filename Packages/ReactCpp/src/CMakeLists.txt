add_library(react_cpp_src
    react-dom/client/ReactDOMComponent.cpp
    react-dom/client/ReactDOMDiffProperties.cpp
    react-dom/client/ReactDOMInstance.cpp
    reconciler/FiberNode.cpp
    react-reconciler/ReactFiberConcurrentUpdates.cpp
    react-reconciler/ReactUpdateQueue.cpp
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
