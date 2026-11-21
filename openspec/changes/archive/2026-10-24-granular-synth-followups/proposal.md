# Proposal: Granular Synth Follow-ups (a-f)

## Intent

Close the six gaps that verify and archive left open for `granular-synth`. Today R-GR-7 and R-GR-8 are only partly covered by tests, the window range is never checked, and no CI runs. Clang and the sanitizers have never compiled or run the core. The goal is to prove the existing contract on two toolchains, not to add behavior.

## Scope

### In Scope
- (a) Test that grain length scales with sample rate: 10 ms gives 221/441/480/960/1920 samples at 22.05k to 192k. Length must not change when the source rate differs from the engine rate.
- (b) Test that pitch is clamped at engine level: +100 equals +24 and -100 equals -24, bit-identical. Negative control: 24 and 23 must differ.
- (c) Strengthen the realtime extremes test: track `maxActive` per block and check that output sounds. At density 0, assert nothing plays.
- (d) Test that window values stay in [0,1] across about 100k phases.
- (e) GitHub Actions CI with two jobs: Windows MSVC (x64 Debug + ctest) and Linux clang (Ninja Debug, ASan + UBSan). Add an opt-in `GRAINS_SANITIZE` CMake option.
- (f) Keep the 0.25 s extremes grid and add a 1 s worst-case subset of about 4 combos.

### Out of Scope (non-goals)
- Plugin target / VST3 (`GRAINS_BUILD_PLUGIN` stays OFF)
- Soft limiter, playback modes
- Merging the stacked PRs
- Comparing outputs across platforms, coverage, macOS job

## Decisions

| Topic | Decision | Rationale |
|---|---|---|
| (f) duration | Hybrid: 0.25 s grid plus a 1 s subset | The pool fills at about 0.13 s. Only the 1 s run exercises release and reacquire churn while the pool is full. Costs about +5% instead of 4x. |
| `float-cast-overflow` | Include | Clang left it out of `undefined` by default. Float-to-index casts (size 1e9 ms, position) are exactly the risk the extremes tests target. |
| R-BH-5 CI/sanitizers | In scope | Puts the CI and sanitizer guarantee in the spec. Small (~10 lines). |

## Capabilities

### New Capabilities
None

### Modified Capabilities
- `grain-rendering`: R-GR-7 "Extremes grid" changes from "1 s each" to "≥0.25 s each plus a 1 s worst-case subset".
- `build-harness`: add R-BH-5, "CI runs MSVC build+tests and clang ASan/UBSan build+tests on push/PR".

## Approach

Strict TDD, one PR, two commits: `test:` for a-d and f, then `ci:` for e. The sanitizer flags are set before `add_subdirectory` so that Catch2 is instrumented too. Workflow: `permissions: contents: read`, with concurrency cancelling superseded runs.

## Affected Areas

| Area | Impact |
|---|---|
| `tests/test_engine_bounds.cpp`, `tests/test_engine_extremes.cpp` | Modified |
| `tests/test_engine_realtime.cpp`, `tests/test_window.cpp` | Modified |
| `CMakeLists.txt` | Modified (`GRAINS_SANITIZE`) |
| `.github/workflows/ci.yml` | New |
| `openspec/specs/{grain-rendering,build-harness}/spec.md` | Delta |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| AllocationCounter conflicts with ASan's new/delete | Med | Fallback: run `ctest -E realtime` in the ASan job; Windows keeps the realtime tests |
| Clang `-Werror` or new MSVC `/W4` warnings | Med | Budget 0-20 lines of fixes |
| `float-cast-overflow` finds real bugs | Med | Fix if small; otherwise drop the flag and log a follow-up |
| Allocation elision in the counter self-test | Low | Debug (-O0); keep the pointer from being optimized away |
| Ubuntu `mmap_rnd_bits` breaks ASan | Low | Use clang ≥18 or the sysctl workaround |
| Flaky (c) thresholds | Low | Use a combined threshold, checked under TDD |

## Rollback Plan

Revert the PR. Each commit can also be reverted on its own: deleting `ci.yml` and the option turns CI off. The tests are additive. The spec deltas are reverted with the archive.

## Dependencies

- GitHub-hosted runners; Catch2 FetchContent (pinned SHA)

## Success Criteria

- [ ] Tests a-d and f pass locally (MSVC) and in both CI jobs
- [ ] Both CI jobs are green on the PR with sanitizers on and no suppressions
- [ ] R-GR-7 is amended and R-BH-5 is added
- [ ] About 260-290 changed lines, within the 800 budget

## Proposal question round (auto mode, assumptions for user review)

1. Can CI be made a required check for merges to main? Assumed: yes, later. Not configured in this PR.
2. If `float-cast-overflow` finds a real engine bug, should the fix go in this PR? Assumed: yes, if it is under about 30 lines.
3. Is it acceptable for the ASan job to skip the realtime tests if the allocation counter conflicts with ASan? Assumed: yes, as long as Windows still runs them.
