cmake_minimum_required(VERSION 3.13)
project(ReactCppWebDemo)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  message(FATAL_ERROR "This demo must be built with Emscripten. Configure with emcmake.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REACT_CPP_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../packages/ReactCpp/src)
include(${REACT_CPP_SOURCE_DIR}/ReactCppSources.cmake)

add_executable(react_wasm_web_demo
  BrowserHostInterface.cpp
  BrowserDemo.cpp
  BrowserMain.cpp
  WasmLayoutBuilder.cpp
  ${REACT_CPP_SOURCE_FILES}
)

target_include_directories(react_wasm_web_demo PRIVATE
  ${REACT_CPP_SOURCE_DIR}
  ${REACT_CPP_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../packages/ReactCpp/test
  ${CMAKE_CURRENT_SOURCE_DIR}/../../jsi
)

target_link_options(react_wasm_web_demo PRIVATE
  "-sEXPORTED_FUNCTIONS=['_react_browser_bootstrap','_react_browser_render_step','_react_browser_render_layout','_react_browser_clear']"
  "-sEXPORTED_RUNTIME_METHODS=['cwrap','ccall','UTF8ToString','stringToUTF8','lengthBytesUTF8','stackSave','stackRestore','stackAlloc']"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sMODULARIZE=1"
  "-sEXPORT_NAME=createReactRuntimeModule"
  "-sENVIRONMENT=web"
)

set_target_properties(react_wasm_web_demo PROPERTIES
  OUTPUT_NAME "react_wasm_web_demo"
  SUFFIX ".js"
)
