add_library(grains_core STATIC
  src/ParamLimits.cpp
  src/GrainScheduler.cpp
  src/GranularEngine.cpp
  src/SampleBuffer.cpp
  src/SampleBuilder.cpp
  src/WindowTable.cpp)
target_include_directories(grains_core PUBLIC include)
target_compile_features(grains_core PUBLIC cxx_std_17)
grains_set_warnings(grains_core)

# R-BH-1: the DSP core must stay JUCE-free. The check is deferred to the end of the
# top-level configure so links added after this point are seen too, and it inspects both
# LINK_LIBRARIES and INTERFACE_LINK_LIBRARIES (recursing through non-JUCE targets).
function(grains_assert_juce_free target)
  foreach(_prop LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
    get_target_property(_libs ${target} ${_prop})
    if(NOT _libs)
      continue()
    endif()
    foreach(_lib IN LISTS _libs)
      if(_lib MATCHES "(^|[:>])juce")
        message(FATAL_ERROR "grains_core must not link JUCE targets (found: ${_lib})")
      endif()
      if(TARGET ${_lib})
        get_property(_seen GLOBAL PROPERTY GRAINS_JUCE_SCAN_${_lib})
        if(NOT _seen)
          set_property(GLOBAL PROPERTY GRAINS_JUCE_SCAN_${_lib} TRUE)
          grains_assert_juce_free(${_lib})
        endif()
      endif()
    endforeach()
  endforeach()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL grains_assert_juce_free grains_core)
