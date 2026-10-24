# Build Harness Specification

## Purpose

A JUCE-free CMake build for the DSP core and its Catch2 test suite, run with `ctest`, kept isolated from the Projucer plugin files.

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

### R-BH-5 CI and sanitizers
The repository MUST provide a GitHub Actions workflow that runs on push to `main` and on every `pull_request` (no branch filter on `pull_request`) with two jobs: (1) Windows MSVC, x64 Debug build plus ctest; (2) Linux clang, Ninja Debug build plus ctest with AddressSanitizer and UndefinedBehaviorSanitizer (including float-cast-overflow) enabled. Sanitizers MUST be controlled by an opt-in CMake option `GRAINS_SANITIZE` (default OFF) whose flags apply to all targets including Catch2, and MUST NOT require suppression files. The workflow MUST use least-privilege permissions (`contents: read`) and cancel superseded runs of the same ref. If the allocation counter proves incompatible with ASan's operator new/delete, the sanitizer job MAY exclude the realtime tests, provided the Windows job still runs them. Making CI a required check for merges to main is out of scope for this change. The default configure/build (option OFF) MUST be unchanged and JUCE-free (R-BH-1), and plugin targets remain off (R-BH-3).

#### Scenario: Both jobs run on push to main and on PRs
- Given `.github/workflows/ci.yml` is present
- When a push to `main` or any pull_request event occurs (no branch filter on pull_request)
- Then a Windows MSVC job and a Linux clang sanitizer job run, each building and running `ctest` to completion

#### Scenario: Sanitizer option is opt-in
- Given a fresh configure without `GRAINS_SANITIZE`
- When the project is built and tested
- Then no sanitizer flags are applied and the suite is green
- And with `-DGRAINS_SANITIZE=ON` on clang, ASan and UBSan flags apply to the core, tests and Catch2

#### Scenario: Sanitizer build is clean
- Given the clang sanitizer job
- When the full suite runs
- Then there are no ASan or UBSan reports and no suppressions are used

#### Scenario: Workflow hardening
- Given the workflow file
- When inspected
- Then it declares `permissions: contents: read` and a concurrency group that cancels in-progress runs for the same ref

#### Scenario: Realtime exclusion fallback
- Given the AllocationCounter conflicts with ASan
- When the sanitizer job runs
- Then it MAY run `ctest -E "^realtime:"`, and the Windows job still runs the realtime tests
