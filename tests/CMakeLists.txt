# Catch2 is fetched at a pinned commit by default so every build uses the same bytes. An
# installed Catch2 is never picked up implicitly (no FIND_PACKAGE_ARGS). Offline options:
#   -DFETCHCONTENT_SOURCE_DIR_CATCH2=<path>   use a local checkout (honoured by FetchContent)
#   -DGRAINS_USE_SYSTEM_CATCH2=ON             explicitly opt in to an installed Catch2 3.x
option(GRAINS_USE_SYSTEM_CATCH2 "Use an installed Catch2 3.x instead of the pinned fetch" OFF)

if(GRAINS_USE_SYSTEM_CATCH2)
  find_package(Catch2 3 REQUIRED)
else()
  include(FetchContent)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    # v3.8.1, pinned to the commit SHA so a moved tag cannot change the build.
    GIT_TAG        2b60af89e23d28eefc081bc930831ee9d45ea58b
    SYSTEM)
  FetchContent_MakeAvailable(Catch2)
endif()

# catch_discover_tests lives in Catch.cmake: shipped in extras/ of a source tree; an installed
# package already makes it available through find_package.
if(catch2_SOURCE_DIR)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
include(Catch)

add_executable(grains_tests
  test_harness.cpp
  test_params_limits.cpp
  test_window.cpp
  test_rng.cpp
  test_sample_buffer.cpp
  test_pool.cpp
  test_scheduler.cpp
  test_interpolator.cpp
  test_grain_render.cpp
  test_params.cpp
  test_engine_bounds.cpp
  test_engine_determinism.cpp
  test_engine_extremes.cpp
  test_engine_loudness.cpp
  test_engine_pool_full.cpp
  test_param_spec.cpp)
target_include_directories(grains_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(grains_tests PRIVATE grains_core Catch2::Catch2WithMain)
grains_set_warnings(grains_tests)
catch_discover_tests(grains_tests)

# R-GR-8 zero-allocation tests live in their own executable: AllocationCounter.cpp replaces the
# global operator new/delete, which must not affect the shared grains_tests binary.
add_executable(grains_realtime_tests
  support/AllocationCounter.cpp
  test_engine_realtime.cpp)
target_include_directories(grains_realtime_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(grains_realtime_tests PRIVATE grains_core Catch2::Catch2WithMain)
grains_set_warnings(grains_realtime_tests)
catch_discover_tests(grains_realtime_tests)
