# Build Harness Specification (delta)

## MODIFIED Requirements

### R-BH-3 Plugin target isolation
Plugin-related targets (JUCE, VST3, Standalone) MUST be created only when option `GRAINS_BUILD_PLUGIN` is `ON`; the option MUST default to `OFF`. With the default configuration no JUCE target MAY be defined and the build MUST stay JUCE-free (R-BH-1). When `ON`, CMake MUST resolve JUCE in this order: (1) the cache path `GRAINS_JUCE_DIR` (default `../../Projucer`, relative to the repository root, overridable by the user) if it contains a JUCE `CMakeLists.txt`; (2) otherwise, if option `GRAINS_JUCE_FETCH` is `ON` (default `ON`), a fetched copy of JUCE 9.0.0 pinned to commit `f8f8864172464b9adf9eba6101e1f784838d1597` (tag `9.0.0`), downloaded as a URL tarball verified with `URL_HASH SHA256=<digest>`; (3) otherwise, if `GRAINS_JUCE_FETCH` is `OFF`, configuration MUST fail with a clear error naming the path and the variable `GRAINS_JUCE_DIR`. The fetch MUST be download-only (no automatic `add_subdirectory` by the fetch mechanism); the resolved JUCE directory, local or fetched, MUST be added with `add_subdirectory(... EXCLUDE_FROM_ALL)`. A failed fetch (including a hash mismatch) MUST fail configuration with a clear error. The plugin MUST link `grains_core`; `grains_core` MUST NOT link the plugin or JUCE. Existing Projucer files (`Grains.jucer`, `Builds/`, `JuceLibraryCode/`) MUST remain untouched.

(Previously: "...When `ON`, CMake MUST locate JUCE via the cache path `GRAINS_JUCE_DIR` ... and MUST add it with `add_subdirectory(... EXCLUDE_FROM_ALL)`. If `GRAINS_JUCE_DIR` does not contain a JUCE `CMakeLists.txt`, configuration MUST fail with a clear error naming the path and the variable. No FetchContent JUCE fallback is provided. ... The CI job defined by R-BH-5 MUST keep `GRAINS_BUILD_PLUGIN` OFF (no plugin CI job in this change).")

#### Scenario: Default configure has no plugin [Manual: configure; automatic via existing CI]
- Given a clean build directory and no JUCE checkout reachable
- When CMake is configured with default options
- Then configuration succeeds, no JUCE or plugin target is defined, no download is attempted, and the existing tests pass under `ctest`

(Unchanged from previous, apart from the explicit no-download assertion.)

#### Scenario: Plugin ON with local JUCE present [Manual]
- Given `GRAINS_JUCE_DIR` points at a valid JUCE checkout
- When CMake is configured with `-DGRAINS_BUILD_PLUGIN=ON` and the plugin target is built
- Then VST3 and Standalone are produced, nothing is downloaded, and `grains_core` still has no JUCE dependency

(Previously: "Plugin ON with JUCE present". Now also asserts that the local directory takes precedence over the fetch, even with `GRAINS_JUCE_FETCH=ON`.)

#### Scenario: Plugin ON with JUCE missing, fetch enabled [CI]
- Given `GRAINS_JUCE_DIR` points to a path without JUCE and `GRAINS_JUCE_FETCH` is `ON`
- When CMake is configured with `-DGRAINS_BUILD_PLUGIN=ON`
- Then the pinned JUCE tarball is downloaded and its SHA256 verified, JUCE is added with `EXCLUDE_FROM_ALL`, and `grains_plugin` builds

(Previously: "Plugin ON with JUCE missing" failed configuration unconditionally.)

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

(Previously: applied to the local JUCE only.)

#### Scenario: Projucer files unchanged
- Given the change is applied
- When the diff against the base commit is inspected for `Grains.jucer`, `Builds/` and `JuceLibraryCode/`
- Then there are no modifications

(Previously: also required the CI workflow to be unmodified; that clause is removed because R-BH-5 now adds a job.)

### R-BH-5 CI and sanitizers
The repository MUST provide a GitHub Actions workflow that runs on push to `main` and on every `pull_request` (no branch filter on `pull_request`) with three jobs: (1) Windows MSVC, x64 Debug build plus ctest; (2) Linux clang, Ninja Debug build plus ctest with AddressSanitizer and UndefinedBehaviorSanitizer (including float-cast-overflow) enabled; (3) an informational Windows MSVC plugin job that configures with `GRAINS_BUILD_PLUGIN=ON`, leaves `GRAINS_JUCE_DIR` at its default (absent on the runner) so the pinned fetch path of R-BH-3 is exercised, and builds the `grains_plugin` target. The plugin job MUST NOT run tests and MUST NOT be a required check for merges to `main`; it SHOULD cache the JUCE download keyed on the pinned commit SHA and MUST set a job timeout. Jobs (1) and (2) MUST keep `GRAINS_BUILD_PLUGIN` OFF. Every third-party action used by the workflow, including in the plugin job, MUST be pinned by full commit SHA. Sanitizers MUST be controlled by an opt-in CMake option `GRAINS_SANITIZE` (default OFF) whose flags apply to all targets including Catch2, and MUST NOT require suppression files. The workflow MUST use least-privilege permissions (`contents: read`) and cancel superseded runs of the same ref. If the allocation counter proves incompatible with ASan's operator new/delete, the sanitizer job MAY exclude the realtime tests, provided the Windows job still runs them. Making CI a required check for merges to main is out of scope. The default configure/build (option OFF) MUST be unchanged and JUCE-free (R-BH-1).

(Previously: "...with two jobs: (1) Windows MSVC ... (2) Linux clang ..." and "plugin targets remain off (R-BH-3)". No plugin CI job existed.)

#### Scenario: Both jobs run on push to main and on PRs
- Given `.github/workflows/ci.yml` is present
- When a push to `main` or any pull_request event occurs (no branch filter on pull_request)
- Then a Windows MSVC job and a Linux clang sanitizer job run, each building and running `ctest` to completion

(Unchanged from previous; the plugin job is covered below.)

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

(Unchanged from previous.)

#### Scenario: Sanitizer build is clean
- Given the clang sanitizer job
- When the full suite runs
- Then there are no ASan or UBSan reports and no suppressions are used

(Unchanged from previous.)

#### Scenario: Workflow hardening
- Given the workflow file
- When inspected
- Then it declares `permissions: contents: read`, a concurrency group that cancels in-progress runs for the same ref, and every `uses:` reference is pinned by a full 40-character commit SHA

(Previously: did not require SHA pinning in the spec.)

#### Scenario: Realtime exclusion fallback
- Given the AllocationCounter conflicts with ASan
- When the sanitizer job runs
- Then it MAY run `ctest -E "^realtime:"`, and the Windows job still runs the realtime tests

(Unchanged from previous.)
