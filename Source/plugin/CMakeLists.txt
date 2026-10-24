# Silent VST3 + Standalone shell around grains_core. Metadata below is placeholder until the final
# plugin identity is decided. JUCE comes from the root CMakeLists (local GRAINS_JUCE_DIR or pinned fetch).
juce_add_plugin(GrainsPlugin
  COMPANY_NAME "Grains"
  PLUGIN_MANUFACTURER_CODE Grns
  PLUGIN_CODE Grn1
  PRODUCT_NAME "Grains"
  BUNDLE_ID com.grains.grains
  FORMATS VST3 Standalone
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(GrainsPlugin PRIVATE
  PluginEditor.cpp
  PluginEditor.h
  PluginProcessor.cpp
  PluginProcessor.h
  SampleLoader.cpp
  SampleLoader.h)

target_compile_definitions(GrainsPlugin PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

# Warnings: JUCE's recommended flags only. grains_set_warnings (/WX, -Werror) is for first-party
# code; JUCE module sources compile inside this target.
target_link_libraries(GrainsPlugin
  PRIVATE
    grains_core
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
