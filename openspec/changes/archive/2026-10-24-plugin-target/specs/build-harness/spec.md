# Delta for Build Harness

## MODIFIED Requirements

### R-BH-3 Plugin target isolation
Plugin-related targets (JUCE, VST3, Standalone) MUST be created only when option `GRAINS_BUILD_PLUGIN` is `ON`; the option MUST default to `OFF`. With the default configuration no JUCE target MAY be defined and the build MUST stay JUCE-free (R-BH-1). When `ON`, CMake MUST locate JUCE via the cache path `GRAINS_JUCE_DIR` (default `../../Projucer`, relative to the repository root, overridable by the user) and MUST add it with `add_subdirectory(... EXCLUDE_FROM_ALL)`. If `GRAINS_JUCE_DIR` does not contain a JUCE `CMakeLists.txt`, configuration MUST fail with a clear error naming the path and the variable. No FetchContent JUCE fallback is provided. The plugin MUST link `grains_core`; `grains_core` MUST NOT link the plugin or JUCE. Existing Projucer files (`Grains.jucer`, `Builds/`, `JuceLibraryCode/`) MUST remain untouched. The CI job defined by R-BH-5 MUST keep `GRAINS_BUILD_PLUGIN` OFF (no plugin CI job in this change).
(Previously: plugin-related targets MUST NOT be created in this change.)

#### Scenario: Default configure has no plugin [Manual: configure; automatic via existing CI]
- Given a clean build directory and no JUCE checkout reachable
- When CMake is configured with default options
- Then configuration succeeds, no JUCE or plugin target is defined, and the existing tests plus the new parameter tests pass under `ctest`

#### Scenario: Plugin ON with JUCE present [Manual]
- Given `GRAINS_JUCE_DIR` points at a valid JUCE checkout
- When CMake is configured with `-DGRAINS_BUILD_PLUGIN=ON` and the plugin target is built
- Then VST3 and Standalone are produced and `grains_core` still has no JUCE dependency

#### Scenario: Plugin ON with JUCE missing [Manual]
- Given `GRAINS_JUCE_DIR` points to a path without JUCE
- When CMake is configured with `-DGRAINS_BUILD_PLUGIN=ON`
- Then configuration fails with an error that names the path and `GRAINS_JUCE_DIR`

#### Scenario: JUCE not built by default [Manual]
- Given `GRAINS_BUILD_PLUGIN=ON`
- When the default `all` target is built
- Then JUCE and plugin targets are not built (EXCLUDE_FROM_ALL), unless requested explicitly

#### Scenario: Projucer files and CI unchanged
- Given the change is applied
- When the diff against the base commit is inspected for `Grains.jucer`, `Builds/`, `JuceLibraryCode/` and the CI workflow
- Then there are no modifications
