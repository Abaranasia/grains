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

enable_testing()
add_subdirectory(Source/core)
add_subdirectory(tests)
