# Delta Spec: build-harness (NEW)

## Requirements

### R-BH-1 JUCE-free configuration
The root CMake project MUST configure and build the DSP core library and the test executable without any JUCE source, module, or header being present or referenced. Option `GRAINS_BUILD_PLUGIN` MUST exist and MUST default to `OFF`. The core library MUST be C++17.

#### Scenario: Fresh configure without JUCE
- Given a clean build directory and no JUCE checkout reachable
- When CMake is configured with default options
- Then configuration succeeds and no JUCE target is defined

#### Scenario: Core has no JUCE dependency
- Given the core library target
- When its link dependencies and include paths are inspected
- Then no JUCE target or JUCE include directory appears

### R-BH-2 Test runner
The project MUST provide a Catch2 v3 test executable (fetched at a pinned version) registered with CTest so that `ctest` discovers and runs every test case. A failing test case MUST make `ctest` return non-zero.

#### Scenario: Green suite
- Given a configured and built tree
- When `ctest` is run
- Then all discovered tests pass and the exit code is 0

#### Scenario: Failing test detected
- Given a deliberately failing test case
- When `ctest` is run
- Then the exit code is non-zero and the failing case is named in the output

### R-BH-3 Plugin target isolation
Plugin-related targets (JUCE, VST3, Standalone) MUST NOT be created in this change, and existing Projucer files (`Grains.jucer`, `Builds/`, `JuceLibraryCode/`) MUST remain untouched.

#### Scenario: Projucer files unchanged
- Given the change is applied
- When the diff against the base commit is inspected for those paths
- Then there are no modifications

### R-BH-4 Project config
`openspec/config.yaml` MUST record `ctest` as the test runner and unit-test layer enabled.

#### Scenario: Runner recorded
- Given the change is applied
- When `openspec/config.yaml` is read
- Then `testing.runner` is `ctest` and `layers.unit` is `true`
