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
Plugin-related targets (JUCE, VST3, Standalone) MUST be created only when option `GRAINS_BUILD_PLUGIN` is `ON`; the option MUST default to `OFF`. With the default configuration no JUCE target MAY be defined and the build MUST stay JUCE-free (R-BH-1). When `ON`, CMake MUST resolve JUCE in this order: (1) the cache path `GRAINS_JUCE_DIR` (default `../../Projucer`, relative to the repository root, overridable by the user) if it contains a JUCE `CMakeLists.txt`; (2) otherwise, if option `GRAINS_JUCE_FETCH` is `ON` (default `ON`), a fetched copy of JUCE 9.0.0 pinned to commit `f8f8864172464b9adf9eba6101e1f784838d1597` (tag `9.0.0`), downloaded as a URL tarball verified with `URL_HASH SHA256=<digest>`; (3) otherwise, if `GRAINS_JUCE_FETCH` is `OFF`, configuration MUST fail with a clear error naming the path and the variable `GRAINS_JUCE_DIR`. The fetch MUST be download-only (no automatic `add_subdirectory` by the fetch mechanism); the resolved JUCE directory, local or fetched, MUST be added with `add_subdirectory(... EXCLUDE_FROM_ALL)`. A failed fetch (including a hash mismatch) MUST fail configuration with a clear error. The plugin MUST link `grains_core`; `grains_core` MUST NOT link the plugin or JUCE. Existing Projucer files (`Grains.jucer`, `Builds/`, `JuceLibraryCode/`) MUST remain untouched.

#### Scenario: Default configure has no plugin [Manual: configure; automatic via existing CI]
- Given a clean build directory and no JUCE checkout reachable
- When CMake is configured with default options
- Then configuration succeeds, no JUCE or plugin target is defined, no download is attempted, and the existing tests pass under `ctest`

#### Scenario: Plugin ON with local JUCE present [Manual]
- Given `GRAINS_JUCE_DIR` points at a valid JUCE checkout
- When CMake is configured with `-DGRAINS_BUILD_PLUGIN=ON` and the plugin target is built
- Then VST3 and Standalone are produced, nothing is downloaded, and `grains_core` still has no JUCE dependency

#### Scenario: Plugin ON with JUCE missing, fetch enabled [CI]
- Given `GRAINS_JUCE_DIR` points to a path without JUCE and `GRAINS_JUCE_FETCH` is `ON`
- When CMake is configured with `-DGRAINS_BUILD_PLUGIN=ON`
- Then the pinned JUCE tarball is downloaded and its SHA256 verified, JUCE is added with `EXCLUDE_FROM_ALL`, and `grains_plugin` builds

#### Scenario: Plugin ON with JUCE missing, fetch disabled [Manual]
- Given `GRAINS_JUCE_DIR` points to a path without JUCE
- When CMake is configured with `-DGRAINS_BUILD_PLUGIN=ON -DGRAINS_JUCE_FETCH=OFF`
- Then configuration fails with an error that names the path and `GRAINS_JUCE_DIR`, and no download is attempted

#### Scenario: Fetch failure or hash mismatch fails clearly [Manual]
- Given `GRAINS_JUCE_FETCH` is `ON`, JUCE is not local, and the download is unreachable or its digest differs from `URL_HASH`
- When CMake is configured with `-DGRAINS_BUILD_PLUGIN=ON`
- Then configuration fails with a clear error and no partially-fetched JUCE is used

#### Scenario: Fetch option defaults and has no effect when plugin OFF [Manual]
- Given a fresh cache
- When `GRAINS_JUCE_FETCH` is inspected, and CMake is configured with default options
- Then it is `ON`, and it has no effect because `GRAINS_BUILD_PLUGIN` is `OFF`

#### Scenario: JUCE not built by default [Manual]
- Given `GRAINS_BUILD_PLUGIN=ON`, with a local or a fetched JUCE
- When the default `all` target is built
- Then JUCE and plugin targets are not built (EXCLUDE_FROM_ALL), unless requested explicitly

#### Scenario: Projucer files unchanged
- Given the change is applied
- When the diff against the base commit is inspected for `Grains.jucer`, `Builds/` and `JuceLibraryCode/`
- Then there are no modifications

### R-BH-4 Project config
`openspec/config.yaml` MUST record `ctest` as the test runner and unit-test layer enabled.

#### Scenario: Runner recorded
- Given the change is applied
- When `openspec/config.yaml` is read
- Then `testing.runner` is `ctest` and `layers.unit` is `true`

### R-BH-5 CI and sanitizers
The repository MUST provide a GitHub Actions workflow that runs on push to `main` and on every `pull_request` (no branch filter on `pull_request`) with three jobs: (1) Windows MSVC, x64 Debug build plus ctest; (2) Linux clang, Ninja Debug build plus ctest with AddressSanitizer and UndefinedBehaviorSanitizer (including float-cast-overflow) enabled; (3) an informational Windows MSVC plugin job that configures with `GRAINS_BUILD_PLUGIN=ON`, leaves `GRAINS_JUCE_DIR` at its default (absent on the runner) so the pinned fetch path of R-BH-3 is exercised, and builds the `grains_plugin` target. The plugin job MUST NOT run tests and MUST NOT be a required check for merges to `main`; it SHOULD cache the JUCE download keyed on the pinned commit SHA and MUST set a job timeout. A cache hit restores the fetched sources without repeating the `URL_HASH` check; only a cache miss performs a verified download. Jobs (1) and (2) MUST keep `GRAINS_BUILD_PLUGIN` OFF. Every third-party action used by the workflow, including in the plugin job, MUST be pinned by full commit SHA. Sanitizers MUST be controlled by an opt-in CMake option `GRAINS_SANITIZE` (default OFF) whose flags apply to all targets including Catch2, and MUST NOT require suppression files. The workflow MUST use least-privilege permissions (`contents: read`) and cancel superseded runs of the same ref. If the allocation counter proves incompatible with ASan's operator new/delete, the sanitizer job MAY exclude the realtime tests, provided the Windows job still runs them. Branch protection that makes CI a required check for merges to `main` is not specified here. The default configure/build (option OFF) MUST be unchanged and JUCE-free (R-BH-1).

#### Scenario: All three jobs run on push to main and on PRs
- Given `.github/workflows/ci.yml` is present
- When a push to `main` or any pull_request event occurs (no branch filter on pull_request)
- Then a Windows MSVC job and a Linux clang sanitizer job run, each building and running `ctest` to completion, and the informational Windows MSVC plugin job runs and builds `grains_plugin` without tests

#### Scenario: Plugin job builds through the fetch path [CI]
- Given the workflow is triggered and `GRAINS_JUCE_DIR` is absent on the runner
- When the Windows MSVC plugin job configures with `-DGRAINS_BUILD_PLUGIN=ON` and builds `grains_plugin`
- Then the pinned JUCE is fetched and verified, `grains_plugin` builds successfully, and no tests are run by that job

#### Scenario: Plugin job is informational [Manual]
- Given the repository branch protection settings and the workflow
- When the required status checks for `main` are inspected
- Then the plugin job is not listed as required, and the first two jobs are unaffected by its result

#### Scenario: Plugin job is cached and bounded [Manual]
- Given the plugin job definition
- When inspected
- Then it restores and saves the JUCE download keyed on the pinned commit SHA and declares a timeout

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
- Then it declares `permissions: contents: read`, a concurrency group that cancels in-progress runs for the same ref, and every `uses:` reference is pinned by a full 40-character commit SHA

#### Scenario: Realtime exclusion fallback
- Given the AllocationCounter conflicts with ASan
- When the sanitizer job runs
- Then it MAY run `ctest -E "^realtime:"`, and the Windows job still runs the realtime tests
