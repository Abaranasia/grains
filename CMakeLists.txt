cmake_minimum_required(VERSION 3.25)
project(Grains VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(GrainsWarnings)

option(GRAINS_SANITIZE "Build everything with AddressSanitizer + UBSan (clang/gcc only)" OFF)
if(GRAINS_SANITIZE)
  if(MSVC)
    message(FATAL_ERROR "GRAINS_SANITIZE is not supported with MSVC (use a clang/gcc toolchain)")
  endif()
  # Directory-scoped so Catch2 and grains_core are instrumented together. Any report aborts the test.
  add_compile_options(-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all)
endif()

enable_testing()
add_subdirectory(Source/core)
add_subdirectory(tests)

# Opt-in JUCE plugin (VST3 + Standalone). OFF by default: the default build stays CXX-only and
# JUCE-free. Use a separate build directory (e.g. build-plugin) when turning this on.
option(GRAINS_BUILD_PLUGIN "Build the JUCE VST3 + Standalone plugin (local JUCE or pinned fetch)" OFF)
set(GRAINS_JUCE_DIR "${CMAKE_SOURCE_DIR}/../../Projucer" CACHE PATH
  "Path to a JUCE checkout containing CMakeLists.txt (used when GRAINS_BUILD_PLUGIN=ON)")
option(GRAINS_JUCE_FETCH "Download pinned JUCE 9.0.0 when GRAINS_JUCE_DIR has no JUCE" ON)
if(GRAINS_BUILD_PLUGIN)
  if(EXISTS "${GRAINS_JUCE_DIR}/CMakeLists.txt")
    set(_grains_juce_src "${GRAINS_JUCE_DIR}")
    message(STATUS "Grains: using local JUCE at '${GRAINS_JUCE_DIR}'")
  elseif(GRAINS_JUCE_FETCH)
    message(STATUS "Grains: no JUCE at '${GRAINS_JUCE_DIR}'; fetching JUCE 9.0.0 "
      "(f8f8864172464b9adf9eba6101e1f784838d1597). On failure set -DGRAINS_JUCE_DIR=<JUCE checkout> "
      "or -DGRAINS_JUCE_FETCH=OFF.")
    include(FetchContent)
    FetchContent_Declare(juce
      # JUCE tag 9.0.0
      URL https://github.com/juce-framework/JUCE/archive/f8f8864172464b9adf9eba6101e1f784838d1597.tar.gz
      URL_HASH SHA256=0bef95e4d4e6e8e1aaa98dfa9aae7529235152e140a7710525bc38b03f8c93a9
      SOURCE_SUBDIR grains-download-only) # no CMakeLists.txt there: download only, no add_subdirectory
    FetchContent_MakeAvailable(juce)
    if(NOT EXISTS "${juce_SOURCE_DIR}/CMakeLists.txt")
      message(FATAL_ERROR "Grains: fetched JUCE not available at '${juce_SOURCE_DIR}' (offline, "
        "FETCHCONTENT_FULLY_DISCONNECTED, or FETCHCONTENT_* override?). "
        "Set -DGRAINS_JUCE_DIR=<JUCE checkout> or -DGRAINS_JUCE_FETCH=OFF.")
    endif()
    set(_grains_juce_src "${juce_SOURCE_DIR}")
  else()
    message(FATAL_ERROR
      "GRAINS_JUCE_DIR='${GRAINS_JUCE_DIR}' does not contain a JUCE CMakeLists.txt and "
      "GRAINS_JUCE_FETCH=OFF. Set -DGRAINS_JUCE_DIR=<path to a JUCE checkout>, "
      "or set GRAINS_JUCE_FETCH=ON, or leave GRAINS_BUILD_PLUGIN=OFF.")
  endif()
  enable_language(C) # JUCE's CMake requires the C language to be enabled
  add_subdirectory("${_grains_juce_src}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
  add_subdirectory(Source/plugin EXCLUDE_FROM_ALL)
  # Not part of `all`. Also makes the excluded plugin targets visible to Visual Studio solutions,
  # which otherwise drop them and break `cmake --build --target GrainsPlugin_VST3`.
  add_custom_target(grains_plugin DEPENDS GrainsPlugin_VST3 GrainsPlugin_Standalone)
endif()
