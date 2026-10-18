include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  # v3.8.1, pinned to the commit SHA so a moved tag cannot change the build.
  GIT_TAG        2b60af89e23d28eefc081bc930831ee9d45ea58b
  SYSTEM
  # Offline: -DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=ALWAYS uses an installed Catch2 3.x,
  # or -DFETCHCONTENT_SOURCE_DIR_CATCH2=<path> uses a local checkout.
  FIND_PACKAGE_ARGS 3)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(grains_tests
  test_harness.cpp
  test_params_limits.cpp
  test_window.cpp
  test_rng.cpp
  test_sample_buffer.cpp
  test_pool.cpp
  test_scheduler.cpp)
target_include_directories(grains_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(grains_tests PRIVATE grains_core Catch2::Catch2WithMain)
grains_set_warnings(grains_tests)
catch_discover_tests(grains_tests)
