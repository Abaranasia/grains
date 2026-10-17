add_library(grains_core STATIC
  src/ParamLimits.cpp
  src/WindowTable.cpp)
target_include_directories(grains_core PUBLIC include)
target_compile_features(grains_core PUBLIC cxx_std_17)
grains_set_warnings(grains_core)

# R-BH-1: the DSP core must stay JUCE-free.
get_target_property(_grains_core_libs grains_core LINK_LIBRARIES)
foreach(_lib IN LISTS _grains_core_libs)
  if(_lib MATCHES "^juce")
    message(FATAL_ERROR "grains_core must not link JUCE targets (found: ${_lib})")
  endif()
endforeach()
