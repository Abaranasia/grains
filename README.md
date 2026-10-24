# Grains

Granular synth engine (`grains_core`, JUCE-free) plus an opt-in JUCE plugin shell.

## Build and test (default, no JUCE)

    cmake -S . -B build -DGRAINS_BUILD_PLUGIN=OFF
    cmake --build build --config Debug
    ctest --test-dir build -C Debug --output-on-failure

## Plugin (VST3 + Standalone, opt-in)

The plugin is OFF by default and CI keeps it OFF. It needs a local JUCE checkout. Use a separate
build directory:

    cmake -S . -B build-plugin -DGRAINS_BUILD_PLUGIN=ON -DGRAINS_JUCE_DIR=<path-to-JUCE>
    cmake --build build-plugin --config Debug --target GrainsPlugin_VST3
    cmake --build build-plugin --config Debug --target GrainsPlugin_Standalone

- `GRAINS_JUCE_DIR` defaults to `../../Projucer` relative to the repository root. Configuration
  fails with a clear error if it has no `CMakeLists.txt`.
- The plugin targets are excluded from `all`; build them by target name (or `grains_plugin` for both).
- `GRAINS_SANITIZE=ON` together with the plugin is unsupported.
- The current plugin is a silent shell: 9 automatable parameters, generic editor, no sample loader.
