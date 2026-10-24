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
- The plugin granulates a loaded sample: use the Load button or drop a `.wav`/`.aiff`/`.aif` file
  on the editor. With no sample it outputs silence. Files longer than 5 minutes are truncated.
- The engine plays continuously once a sample loads (no MIDI gating) and has no limiter (output
  can reach +12 dBFS), so start with a low monitoring level.
- Sessions store the sample's absolute path, not the audio. If the file moves, the plugin stays
  silent and shows "Missing: <name>".
