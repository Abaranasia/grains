include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.8.1
  GIT_SHALLOW    TRUE
  SYSTEM)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(grains_tests
  test_harness.cpp
  test_params_limits.cpp
  test_window.cpp
  test_rng.cpp)
target_link_libraries(grains_tests PRIVATE grains_core Catch2::Catch2WithMain)
grains_set_warnings(grains_tests)
catch_discover_tests(grains_tests)
