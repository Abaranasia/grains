# Delta for Build Harness

## ADDED Requirements

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
