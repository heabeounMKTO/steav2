cmake_minimum_required(VERSION 3.12)
project(
  steav2
  VERSION 0.2
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_STEAV_EXAMPLES "Build the steav CLI"                   ON)
option(BUILD_STEAV_LSP      "Build steav2-lsp, the language server" ON)
option(BUILD_STEAV_WASM     "Build steav-wasm.js/.wasm via Emscripten, exposing steav_run(source)" OFF)
option(BUILD_STEAV_TESTS    "Register tests/ with ctest"            OFF)
option(STEAV_DEBUG_TRACE    "Print disassembly per executed op"     OFF)
option(STEAV_DEBUG_PRINT    "Disassemble chunks after compiling"    OFF)
option(STEAV_ASAN           "Build with AddressSanitizer + UBSan"   OFF)
option(STEAV_DEBUG_STRESS_GC "Collect garbage on every allocation"  OFF)

# Debug unless told otherwise (single-config generators only)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    Debug Release RelWithDebInfo MinSizeRel)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Platform detection
if(WIN32)
  set(STEAV_PLATFORM "WINDOWS")
elseif(APPLE)
  set(STEAV_PLATFORM "APPLE")
elseif(UNIX)
  set(STEAV_PLATFORM "UNIX")
else()
  set(STEAV_PLATFORM "UNKNOWN")
endif()
message(STATUS "Detected platform: ${STEAV_PLATFORM}")


file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  ${PROJECT_SOURCE_DIR}/src/frontend/*.cpp
  ${PROJECT_SOURCE_DIR}/src/typecheck/*.cpp
  ${PROJECT_SOURCE_DIR}/src/compiler/*.cpp
  ${PROJECT_SOURCE_DIR}/src/vm/*.cpp
  ${PROJECT_SOURCE_DIR}/src/debug/*.cpp
  ${PROJECT_SOURCE_DIR}/src/stdlib/*.cpp
)

add_library(${PROJECT_NAME} STATIC ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
  set(STEAV_WARNINGS /W4)
else()
  # natives often ignore their args (random.uniform etc) so no unused-parameter
  set(STEAV_WARNINGS -Wall -Wextra -Wno-unused-parameter)
endif()
target_compile_options(${PROJECT_NAME} PRIVATE ${STEAV_WARNINGS})

if(STEAV_ASAN)
  if(MSVC)
    message(FATAL_ERROR "STEAV_ASAN is only set up for clang/gcc")
  endif()
  message(STATUS "ASan + UBSan enabled")
  target_compile_options(${PROJECT_NAME} PUBLIC
    -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=address,undefined)
endif()

if(STEAV_DEBUG_TRACE)
  message(STATUS "Execution tracing enabled")
  target_compile_definitions(${PROJECT_NAME} PUBLIC STEAV_DEBUG_TRACE)
endif()

if(STEAV_DEBUG_STRESS_GC)
  message(STATUS "GC stress mode enabled")
  target_compile_definitions(${PROJECT_NAME} PUBLIC STEAV_DEBUG_STRESS_GC)
endif()

if(STEAV_DEBUG_PRINT)
  message(STATUS "Chunk disassembly enabled")
  target_compile_definitions(${PROJECT_NAME} PUBLIC STEAV_DEBUG_PRINT)
endif()

if(NOT WIN32)
  target_link_libraries(${PROJECT_NAME} PUBLIC m)
endif()

# steav --version and steav-wasm's steav_version() both want this
set(STEAV_GIT_COMMIT "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE STEAV_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET RESULT_VARIABLE git_result)
  if(NOT git_result EQUAL 0)
    set(STEAV_GIT_COMMIT "unknown")
  else()
    execute_process(COMMAND ${GIT_EXECUTABLE} diff --quiet HEAD
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} RESULT_VARIABLE git_dirty ERROR_QUIET)
    if(NOT git_dirty EQUAL 0)
      string(APPEND STEAV_GIT_COMMIT "-dirty")
    endif()
  endif()
endif()

if(BUILD_STEAV_EXAMPLES)
  add_executable(steav ${PROJECT_SOURCE_DIR}/examples/steav.cpp)
  target_link_libraries(steav PUBLIC ${PROJECT_NAME})
  # everything `steav --version` reports, captured at configure time
  set(STEAV_BUILD_OPTIONS "")
  foreach(opt STEAV_DEBUG_TRACE STEAV_DEBUG_PRINT STEAV_DEBUG_STRESS_GC STEAV_ASAN)
    if(${opt})
      string(APPEND STEAV_BUILD_OPTIONS " ${opt}")
    endif()
  endforeach()
  string(STRIP "${STEAV_BUILD_OPTIONS}" STEAV_BUILD_OPTIONS)
  if(STEAV_BUILD_OPTIONS STREQUAL "")
    set(STEAV_BUILD_OPTIONS "none")
  endif()
  set(STEAV_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
  if(STEAV_BUILD_TYPE STREQUAL "")
    set(STEAV_BUILD_TYPE "multi-config")
  endif()
  configure_file(${PROJECT_SOURCE_DIR}/examples/build_info.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/build_info.h @ONLY)
  target_include_directories(steav PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
  target_compile_options(steav PRIVATE ${STEAV_WARNINGS})
endif()

if(BUILD_STEAV_LSP)
  add_executable(steav2-lsp
    ${PROJECT_SOURCE_DIR}/lsp/steav2_lsp.cpp
    ${PROJECT_SOURCE_DIR}/lsp/lsp_core.cpp)
  target_link_libraries(steav2-lsp PUBLIC ${PROJECT_NAME})
  target_compile_options(steav2-lsp PRIVATE ${STEAV_WARNINGS})
endif()

if(BUILD_STEAV_WASM)
  if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "BUILD_STEAV_WASM needs the Emscripten toolchain, configure with: emcmake cmake -B build-wasm -DBUILD_STEAV_WASM=ON")
  endif()
  # lsp_core.cpp: diagnostics/hover/completion, shared with steav2-lsp so the
  # playground shows exactly what an editor running the language server does
  add_executable(steav-wasm
    ${PROJECT_SOURCE_DIR}/examples/steav_wasm.cpp
    ${PROJECT_SOURCE_DIR}/lsp/lsp_core.cpp)
  target_link_libraries(steav-wasm PUBLIC ${PROJECT_NAME})
  target_compile_options(steav-wasm PRIVATE ${STEAV_WARNINGS})
  target_compile_definitions(steav-wasm PRIVATE
    "STEAV_WASM_VERSION=\"${PROJECT_VERSION}\""
    "STEAV_WASM_COMMIT=\"${STEAV_GIT_COMMIT}\"")
  set_target_properties(steav-wasm PROPERTIES SUFFIX ".mjs")
  target_link_options(steav-wasm PRIVATE
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME=createSteav"
    "-sENVIRONMENT=worker,node"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=32MB"
    "-sSTACK_SIZE=2MB" # headroom for the recursive-descent parser/checker
    "-sFILESYSTEM=0"
    "-sEXPORTED_FUNCTIONS=_steav_run,_steav_check,_steav_version,_steav_last_stats,_steav_hover,_malloc,_free"
    "-sEXPORTED_RUNTIME_METHODS=UTF8ToString,stringToNewUTF8,HEAP8")
endif()

if(BUILD_STEAV_TESTS)
  if(NOT BUILD_STEAV_EXAMPLES)
    message(FATAL_ERROR "BUILD_STEAV_TESTS runs .sts files through the steav CLI, turn BUILD_STEAV_EXAMPLES on")
  endif()
  enable_testing()
  add_subdirectory(tests)
endif()

install(
  TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}-targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)

if(BUILD_STEAV_EXAMPLES)
  install(TARGETS steav RUNTIME DESTINATION bin)
endif()

if(BUILD_STEAV_LSP)
  install(TARGETS steav2-lsp RUNTIME DESTINATION bin)
endif()

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/include/
  DESTINATION include
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp")

install(
  EXPORT ${PROJECT_NAME}-targets
  FILE ${PROJECT_NAME}Config.cmake
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION lib/cmake/${PROJECT_NAME})
