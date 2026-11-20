# Proposal: Loader follow-ups (items 2-5)

## Intent
Close four follow-ups left by archived `load-time-resampling`. Loader state is JUCE-bound and untestable (R3-001/W2/S3). A stale rate persists after a superseding request fails (R4-001). Non-integer decimation perf is unmeasured. No test covers `OutOfMemory` (S1).

## Scope
### In Scope
- **(2)** Extract host-free `grains::LoadPlanner` (core, `std::string`/`std::mutex`) owning target/latest/settled/published/pending and request/retarget/settle logic. `SampleLoader` keeps the thread and JUCE I/O.
- **(3)** R4-001 rule in the planner: when a still-latest non-Reload request settles as failed and published rate != target, issue one Reload. Never after a failed Reload. Red test first.
- **(4)** Hidden `[.][perf]` cases: 96k->44.1k stereo 300 s; optionally 384k->44.1k. Optimise only if above ~6 s.
- **(5a)** Resampler huge-ratio test forcing a real `bad_alloc` -> `OutOfMemory`, no hooks.
- **(5b)** `armFailure(n)` fail-nth-allocation hook in `AllocationCounter`, used by builder OOM tests in a separate executable, never `grains_tests`.

### Out of Scope
- Manual host checks D.1-D.5.
- Runtime (pitch-shift) aliasing; user-adjustable quality.
- Loader null-Box OOM paths beyond what the planner seam covers.

## Capabilities
### New Capabilities
- None
### Modified Capabilities
- `sample-source`: R-SS-15 gains the stale-rate recovery rule; planner scenarios become automated.
- `sample-resampling`: R-RS-9 adds a measured non-integer budget; new OutOfMemory requirement.
- `build-harness`: fail-injection tests live in a separate executable.

## Approach
Planner first (TDD, `[loadplan]`), then thin `SampleLoader` adapter (`StringArray` <-> `vector<string>`). Keep lock order: `lock_` before gate mutex; `commitGated` callback never takes `lock_`. Perf: measure, then set budget. OOM: ratio size exceeds 128 TB on all platforms.

## Affected Areas
| Area | Impact |
|---|---|
| `Source/core/include/grains/LoadPlanner.h`, `Source/core/src/LoadPlanner.cpp` | New |
| `Source/core/CMakeLists.txt`, `tests/CMakeLists.txt` | Modified |
| `Source/plugin/SampleLoader.{h,cpp}` | Modified |
| `tests/test_load_planner.cpp`, OOM test file | New |
| `tests/test_resampler.cpp`, `tests/support/AllocationCounter.{h,cpp}` | Modified |

## Risks
| Risk | Likelihood | Mitigation |
|---|---|---|
| Refactor breaks lock/gate ordering | Med | Preserve order; planner tests; manual smoke |
| No host-level safety net | Med | Behaviour-parity tests on planner |
| Reload loop on deleted file | Low | Never reissue after failed Reload; test |
| Huge-alloc test platform-sensitive | Low | Exceed 128 TB; tune in apply |
| Fail-injection leaks into `grains_tests` | Low | Separate executable |

## Rollback Plan
Revert PR-A or PR-B independently. PR-B is test-only unless optimised.

## Success Criteria
- [ ] Planner tests pass, including R4-001 red-then-green and no reissue after failed Reload.
- [ ] `SampleLoader` behaviour unchanged (manual smoke).
- [ ] Perf times recorded; budget set from measurement.
- [ ] Resampler and builder `OutOfMemory` paths covered in CI.

## Review Workload Forecast
~500-700 lines, under 800. PR-A (2+3): ~350-450. PR-B (4+5): ~150-260.

## Proposal question round (assumptions)
User declined; exploration defaults: planner in core; reissue only after non-Reload failure; optimise only above ~6 s; 384k case optional.
