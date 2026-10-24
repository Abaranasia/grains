cmake_minimum_required(VERSION 3.25)
project(Grains LANGUAGES CXX)

option(GRAINS_BUILD_PLUGIN "Build the JUCE plugin (not available yet, roadmap #1)" OFF)
if(GRAINS_BUILD_PLUGIN)
  message(FATAL_ERROR "GRAINS_BUILD_PLUGIN is not supported yet (roadmap #1)")
endif()

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
