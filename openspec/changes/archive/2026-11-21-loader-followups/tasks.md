# Tasks: Loader follow-ups (items 2-5)

Test commands: `cmake -S . -B build -A x64 -DGRAINS_BUILD_PLUGIN=OFF`; `cmake --build build --config Debug`; `ctest --test-dir build -C Debug --output-on-failure`.
Spec IDs: R-SS-15 (R4-001), R-SS-16 (LoadPlanner), R-RS-9 (perf), R-RS-10 (OutOfMemory), R-BH-6 (armFailure).

## Phase 1: LoadPlanner foundation, test-first (PR-A)

- [x] 1.1 Create stub `Source/core/include/grains/LoadPlanner.h` (types, class, API per design) and `Source/core/src/LoadPlanner.cpp`; register in `Source/core/CMakeLists.txt`; add `test_load_planner.cpp` to `tests/CMakeLists.txt`. (R-SS-16)
- [x] 1.2 RED `tests/test_load_planner.cpp` `[loadplan]` request/take: generations increase and match `gate().current()`; request copies target; newer request replaces pending; `take` returns once then `nullopt`. (R-SS-16)
- [x] 1.3 GREEN: `request`, `take`, `gate`, `target` with `mutex_` then gate order.
- [x] 1.4 RED retarget: invalid and same rate are no-ops; pending replaced at new rate; in-flight reissued with same fields; Reload carries path/original/displayName/label/rate; none after `cleared()`; `published()` before `settle` still counts as in flight. (R-SS-16)
- [x] 1.5 GREEN: `retarget`, `published`, `cleared` (three branches moved unchanged from `SampleLoader`).

## Phase 2: R4-001 (PR-A)

- [x] 2.1 RED R4-001: `retarget(B)` issues Reload R; take R; `request` U; `settle(R, Aborted)` issues nothing; take U; `settle(U, Failed)` issues Reload at B. (R-SS-15)
- [x] 2.2 RED no-reissue cases: failed Reload, reissued Reload that fails, superseded failure, `target_==0`, published rate equals target, nothing published, `Aborted` while latest, failed Restore after `cleared()`. (R-SS-15)
- [x] 2.3 GREEN: `settle` sets `settled_` and applies the rule (latest, origin != Reload, target_ > 0, published rate != target); returns true when it issued a Reload.
- [x] 2.4 RED+GREEN concurrency: two threads, 10k iterations of `request`/`retarget` vs `take`/`commitIf`/`published`/`settle`; completes (no deadlock), last request wins. (R-SS-16)

## Phase 3: SampleLoader adapter (PR-A)

- [x] 3.1 `Source/plugin/SampleLoader.h`: remove `lock_` and state; own a `grains::LoadPlanner`; `using Origin = grains::LoadOrigin`. `PluginProcessor` untouched. (R-SS-16)
- [x] 3.2 `Source/plugin/SampleLoader.cpp`: delegate request/retarget/take/published/cleared; `loadImpl` returns `LoadOutcome` (Published, Cleared, Failed, Aborted); `load` calls `settle` and `notify()` on true; UTF-8 conversion at edges; `published()`/`cleared()` after `commitGated` returns. (R-SS-15)
- [x] 3.4 Review remediation (R-SS-15): RED `settle(g, Failed, "Error: u.wav")` reissues a Reload labelled with the failure; GREEN `settle` takes `failureStatus`; adapter tracks the run's last post and a Reload success posts `r.label`. JUCE part not unit-testable: manual D.6.
- [x] 3.3 Full ctest green (baseline plus new cases); MSVC /W4 /WX clean; plugin target compiles if JUCE available.

## Phase 4: Perf and OutOfMemory (PR-B, stacked on PR-A)

- [x] 4.1 `tests/test_resampler.cpp`: hidden `[.][perf]` "300 s stereo 96k to 44.1k within 6 s" (`REQUIRE`) and 384k to 44.1k (`WARN` only); run in Release, record times in the R-RS-9 spec. (R-RS-9)
- [x] 4.2 RED+GREEN `[allocfail][resampler]`: fail-nth-allocation sweep over `grains::resample` returns `OutOfMemory`, no channels, no leak, in `grains_alloc_failure_tests` (the 1e12 huge-ratio trick was dropped: ASan aborts on absurd requests). Gated by `GRAINS_ALLOC_SWEEPS`. (R-RS-10)

## Phase 5: Fault injection (PR-B)

- [x] 5.1 RED `tests/test_alloc_failure.cpp`: new executable `grains_alloc_failure_tests`, ctest-registered in `tests/CMakeLists.txt`, never in `grains_tests`; builder sweep n=1.. (passthrough, 48k to 44.1k, mono/stereo, 1000 frames): fired gives `OutOfMemory` and null buffer; not fired gives `None` and stop. (R-BH-6, R-RS-10)
- [x] 5.2 GREEN: `armFailure(n)`, `disarmFailure()`, `failureFired()` in `tests/support/AllocationCounter.{h,cpp}` (atomic countdown in `plainAlloc`/`alignedAlloc`, null on zero).
- [x] 5.3 RED+GREEN planner OOM: with failure armed, `retarget` returns false and `target()` unchanged; `settle` issues nothing, does not throw. (R-BH-6, R-SS-16)

## Phase 6: Contingencies

- [x] 6.1 If 5.1 terminates under MSVC Debug (iterator proxy allocation in `noexcept` move): register `[allocfail]` for Release only; note in design Open Questions. (R-BH-6)
- [x] 6.2 (not triggered: 5.4 s) If 4.1 measures 96k to 44.1k above 6 s: separate slice adding per-channel `std::thread` (bit-exact); existing accuracy tests unchanged. (R-RS-9)
- [x] 6.3 Final full ctest; record perf numbers. Manual host checks D.1-D.5 remain out of scope.

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | PR-A ~460, PR-B ~195 (~655 total) |
| 400-line budget risk | High for PR-A (above 400, below 800 review budget); Low for PR-B |
| Chained PRs recommended | Yes |
| Suggested split | PR-A -> PR-B (base = PR-A branch) |
| Delivery strategy | auto-chain (review_budget_lines=800) |
| Chain strategy | stacked-to-main |

Decision needed before apply: No
Chained PRs recommended: Yes
Chain strategy: stacked-to-main
400-line budget risk: High

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | LoadPlanner, R4-001, SampleLoader adapter | PR-A | `ctest --test-dir build -C Debug -R loadplan --output-on-failure` | N/A: plugin needs a host (D.1-D.5 manual, out of scope) | LoadPlanner files, SampleLoader.*, test_load_planner, CMake lines |
| 2 | Perf, OOM, armFailure, alloc-failure exe | PR-B (base = PR-A) | `ctest --test-dir build -C Debug -R "resampler|allocfail" --output-on-failure` | `[.][perf]` Release run of the 96k to 44.1k case | test_resampler additions, AllocationCounter, test_alloc_failure, CMake |
