# Exploration: loader-followups

Engram: `sdd/loader-followups/explore` (observation 693). This file mirrors the phase summary.
Source: follow-ups left open by archived `load-time-resampling` (review R3-001, R4-001, W2; verify S1, S3). Manual host checks D.1-D.5 are out of scope.

## Current state (verified in code)

- **Item 2 (retarget seam):** loader state (`target_`, `latest_`, `settled_`, `published_`, `pending_`) lives in `Source/plugin/SampleLoader.{h,cpp}` under `juce::CriticalSection` and `GenerationGate`, with `juce::StringArray`/`String`. `grains_tests` links only `grains_core`, so none of it is testable. `GenerationGate` is already covered by `tests/test_generation_gate.cpp`.
- **Item 3 (R4-001):** confirmed. `load()` only sets `settled_ = generation`. Sequence: `retarget(B)` issues a Reload; a User load supersedes it and the Reload aborts; the User load fails; `fail()` keeps the buffer and leaves `published_` at the old rate A; a repeated `retarget(B)` is a no-op (`rate == target_`). The stale rate persists until the next rate change. Quality-only; the engine compensates.
- **Item 4 (decimation performance):** each output frame costs two kernel-row dot products when the fractional position is non-zero, one when it is zero. The existing hidden perf test (96k to 48k) is an integer ratio, so it measures the cheap path (2.47 s). Estimates (not measured): 96k to 44.1k about 5 s; 384k to 44.1k on a 300 s file about 20 s. Work runs on the loader thread and is abortable every 65536 frames, so the effect is latency, not audio glitches.
- **Item 5 (OutOfMemory):** no test references `OutOfMemory`. Catch sites: `Resampler.cpp`, two in `SampleBuilder.cpp` plus the status mapping at line 85, and the loader (`setSize`, null staged Boxes). `tests/support/AllocationCounter.cpp` replaces global `operator new` only in `grains_realtime_tests` and cannot fail an allocation.

## Approaches and recommendation

- **Items 2+3 together (recommended):** extract a host-free `grains::LoadPlanner` into core (`std::string`/`std::mutex`) owning request/retarget/settle/published logic. `SampleLoader` keeps the thread and JUCE I/O. R4-001 rule: when a non-Reload request that is still the latest settles as failed and `published_.rate != target_`, issue a Reload. Never reissue after a failed Reload (a deleted file would loop). A failed Restore clears `published_`, so no reissue. Test first (red), then fix. Rejected: templating `SampleLoader` on an I/O policy (heavier); a JUCE-linked test executable (heavy for CI).
- **Item 4 (recommended):** add hidden `[.][perf]` cases for 96k to 44.1k stereo 300 s and optionally 384k to 44.1k; measure, then set the budget. Optimise only if above about 6 s (skip the second row when the fraction is tiny, denser table, or per-channel threads).
- **Item 5 (recommended):** (a) `resample` with 1000 frames, src=1, dst about 1e12 and default `maxFrames` forces a real `bad_alloc` without any hook (numbers tuned during apply). (b) The builder caps output at INT_MAX frames, so add an `armFailure(n)` fail-nth-allocation hook to `AllocationCounter`, in a separate executable, never in `grains_tests`. Loader Box-null paths stay manual unless the planner seam covers them.

## Affected files

- 2+3: new `Source/core/include/grains/LoadPlanner.h`, `Source/core/src/LoadPlanner.cpp`; `Source/core/CMakeLists.txt`; `Source/plugin/SampleLoader.{h,cpp}`; new `tests/test_load_planner.cpp`; `tests/CMakeLists.txt`.
- 4: `tests/test_resampler.cpp`.
- 5: `tests/test_resampler.cpp`, `tests/support/AllocationCounter.{h,cpp}`, new OOM test file, `tests/CMakeLists.txt`.

## Independence and size

Items 2+3 are coupled; 4 and 5 are independent of each other and of 2+3 (mild merge overlap in `test_resampler.cpp`).

| Item | Changed lines |
|---|---|
| 2+3 | ~350-450 |
| 4 | ~30-50 (+40-80 if optimised) |
| 5a | ~25 |
| 5b | ~90-130 |

Total about 500-700, under the 800 budget. Suggested split: PR-A for 2+3, PR-B for 4+5.

## Risks

- Planner refactor must keep lock ordering: `lock_` before the gate mutex; the `commitGated` callback must not take `lock_`.
- Only manual host checks cover loader behaviour today, so the refactor has no host-level safety net.
- Perf figures are estimates; the huge-allocation test must exceed the 128 TB address space on all platforms.
- Fail-injection `operator new` must stay out of the shared `grains_tests` binary.
