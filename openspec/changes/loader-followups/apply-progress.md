# Apply progress: loader-followups (PR-A, Phases 1-3)

Mode: Strict TDD. Status: PR-A complete (12/12 tasks); PR-B (Phases 4-6) complete (tasks 4.1-6.3; 6.2 not triggered).

## Tasks
- [x] 1.1-1.5 LoadPlanner (header, cpp, CMake, request/take/retarget/published/cleared)
- [x] 2.1-2.4 R4-001 in `settle`, no-reissue cases, concurrency test
- [x] 3.1-3.3 SampleLoader adapter, full ctest, warnings clean

## TDD evidence
- RED 1 (tasks 1.2/1.4): stub planner, 11 `[loadplan]` cases, 11/11 failed.
- GREEN 1: request/take/retarget/published/cleared, `settle` only sets `settled_`: 11/11 pass.
- RED 2 (2.1/2.2): 9 R4-001 cases added, 2 failed (positive reissue, reissued Reload that fails); 7 no-reissue cases hold trivially against the stub.
- GREEN 2: `settle` applies the rule, 20/20 pass. Refactor: shared `makeReload` helper.
- 2.4: concurrency regression test added (10k iterations, two threads); passes (0.02-0.2 s, no deadlock).
- Final: 21 `[loadplan]` cases; full ctest 214/214 in Debug and Release (baseline 193 before PR-A + 21 new).
- MSVC /W4 /WX clean in Debug and Release.
- Plugin: configured `build-plugin` with GRAINS_BUILD_PLUGIN=ON (local JUCE at ../../Projucer); `GrainsPlugin_Standalone` compiles and links in Debug and Release under /W4 /WX. First attempt caught a real error (const `latestGeneration()` needed a const `gate()` overload); fixed. Not run: manual host checks D.1-D.5.

## Files
- New: `Source/core/include/grains/LoadPlanner.h`, `Source/core/src/LoadPlanner.cpp`, `tests/test_load_planner.cpp`
- Modified: `Source/core/CMakeLists.txt`, `tests/CMakeLists.txt`, `Source/plugin/SampleLoader.h`, `Source/plugin/SampleLoader.cpp`

## Deviations from design
- Added `const GenerationGate& gate() const` overload (needed by `latestGeneration() const`).
- `retarget`/`settle` wrap the copy in try/catch(...) to honour `noexcept` with "issues nothing, state unchanged"; `request` copies before the lock.
- `loadImpl` outcome mapping: clear-allocation failure and staging failure while current return Failed; rejected commits return Aborted. Restore failure whose clear commit is accepted returns Failed (planner then sees no published source, so no reload).
- Tests are ~348 lines vs ~220 estimated (more cases); total diff about 730 lines touched (about 608 added, 122 removed), within the 800 review budget.

## Verify remediation (W1, S1)
- Added 2 `[loadplan]` cases in `tests/test_load_planner.cpp` (production code untouched): stale-published-rate latest User request settling Published issues nothing (W1); repeated failed-Reload settle issues nothing (S1).
- Mutation proof: `settle` mutated to reissue on Published (`outcome == Aborted ||` guard) -> W1 test failed (2 assertions, 22/23 pass); reverted, `cmp` byte-identical to backup.
- Final: 23 `[loadplan]` cases (121 assertions); full ctest 216/216 Debug and Release; /W4 /WX clean `--clean-first`.

## Review remediation: error status masked by the reissued Reload (R-SS-15)
- Problem: on a failed latest User request, the "Error: <name>" post (generation g) was dropped by `onLoadResult` (`r.generation < latestGeneration()`) because `settle` had already bumped the gate to g+1; the silent Reload then posted the previous file's "Loaded: ..." line.
- RED: `settle(gu, Failed, "Error: u.wav")` added (stub ignoring the argument): 1 new case failed ("carries the failure status"); the default-label case held. GREEN: reissued Reload's `label` = failure status when given.
- Design: `LoadPlanner::settle(gen, outcome, failureStatus = {})`; the reissued Reload carries it as `label`. `SampleLoader::load` passes the failed run's last post text (`lastText_`, loader thread only); `loadImpl` Reload success now posts `r.label` (published label for a retarget Reload, identical to before; failure line for a reissue). Published label untouched. Reload failure already posted `r.label`. No reissue loop, lock order, R4-001 conditions and `PluginProcessor` unchanged.
- Rejected: not bumping the gate (breaks supersede/abort semantics); processor-side status-neutral Reload (touches PluginProcessor, untestable); settle returning info for the adapter to re-post (more plumbing, same effect).
- Tests: 25 `[loadplan]` cases (2 new); full ctest 218/218 Debug and Release; /W4 /WX clean; `GrainsPlugin_Standalone` builds Debug and Release.
- Not unit-testable (JUCE-only: `lastText_` capture, Reload success posting `r.label`): manual check D.6 -- with the host rate differing from a loaded file's rate, load a missing/corrupt file via the UI; the status must stay "Error: <name>" (not "Loaded: <previous>") and the previous sample keeps playing at the target rate.

## PR-B (Phases 4-6)

- [x] 4.1 perf cases; [x] 4.2 resampler OOM; [x] 5.1-5.3 fault injection and planner OOM; [x] 6.1 triggered; [ ] 6.2 not triggered; [x] 6.3 final ctest

### Perf (Release, MSVC, reference machine, 300 s stereo noise)
- 96k to 44.1k: 5.44 s and 5.43 s (two runs); budget 6 s, passes with about 10% margin. Contingency 6.2 NOT triggered.
- 384k to 44.1k: 20.40 s (informational, WARN only, no budget).
- 96k to 48k (existing case): 2.80 s (budget 3 s).
- The 96k to 44.1k case is close to the limit; a slower host could exceed it. It is hidden `[.][perf]` and not part of the CI gate.

### TDD evidence
- 4.2: the test passed on first run (the production code already maps bad_alloc to OutOfMemory; no hook, ratio 1e12 is accepted for upsampling and the default frame limit is SIZE_MAX, so no spec adjustment was needed). Mutation proof: `result.status = OutOfMemory` changed to `InvalidArgument` -> test failed (2 assertions); reverted, `git diff Source/` empty.
- 5.1 RED: `test_alloc_failure.cpp` and the new executable added before the hook: compile errors C2039/C3861 (`armFailure`, `disarmFailure`, `failureFired` not members of grains_test).
- 5.2 GREEN: hook added; Release: 5 cases / 182 assertions pass. First run showed a test bug (settle's `std::string` argument copy was made inside the armed window and threw in the caller); fixed by building and moving the string before arming.
- 5.3: planner retarget (in-flight and published branches) and settle (R4-001 Reload) sweeps pass against existing planner code (its try/catch already satisfies the spec); no production change.
- `disarmFailure()` keeps `failureFired()` (cleared only by `armFailure`), so tests read the flag after disarming.

### Contingency 6.1 (triggered)
In MSVC Debug the builder, planner-retarget and planner-settle sweeps terminate the process (exit 3, no Catch output) at the first allocation failure inside a `noexcept` container move (hypothesis: debug-runtime iterator proxy allocation; not isolated further). They are compiled out under `_MSC_VER && _DEBUG` via `GRAINS_ALLOC_SWEEPS` in `tests/test_alloc_failure.cpp`; the 2 `armFailure` cases run in Debug. Release runs all 5. Design Open Question closed.

### Test counts
- Debug: ctest 220/220 (218 + 2 armFailure cases; the OOM case is gated out in Debug). Release: 224/224 (also the 3 sweep cases). /W4 /WX clean in both, full build.
- grains_tests links neither `AllocationCounter.cpp` nor the hook (only `grains_realtime_tests` and `grains_alloc_failure_tests` do).

### Files (PR-B)
- Modified: `tests/test_resampler.cpp` (+43), `tests/support/AllocationCounter.{h,cpp}` (+9, +32), `tests/CMakeLists.txt` (+11); `design.md`, `tasks.md`
- New: `tests/test_alloc_failure.cpp` (213)
- Changed lines: about 308 added, 0 removed (estimate was ~195: the sweeps, helper builders and the guard comment are larger than forecast). Production code untouched.

### Deviations
- Added two `armFailure` self-tests (nth-fails-once, unarmed counts) from the R-BH-6 scenarios.
- R-RS-9 numbers are recorded here rather than in the spec file.

## PR-B review remediation (resampler OOM test)
- Problem: the 1e12 resampler OOM test requests about 4 PB; ASan's throwing `operator new` is fatal on failure even with `allocator_may_return_null=1`, so the Linux ASan job could abort. The ci.yml `ASAN_OPTIONS` change was reverted (`git diff HEAD -- .github` empty).
- Moved the case to `tests/test_alloc_failure.cpp` as a fail-nth sweep over `grains::resample` (mono and stereo, 48k to 44.1k, 1000 frames): fired run gives `OutOfMemory`, no channels, `allocs == frees`; first unfired run gives `Ok`; plus the pairwise-distinct status check. Removed the old test from `tests/test_resampler.cpp`.
- MSVC Debug: probed a first (ungated) version; it hung at n = 1 (iterator proxy allocation in a noexcept vector constructor reaches `std::terminate`), so the case is gated by `GRAINS_ALLOC_SWEEPS` like the other sweeps. It does NOT run in Debug.
- Mutation proof (Release): `result.status = OutOfMemory` changed to `InvalidArgument` in `Resampler.cpp` -> resampler and builder sweeps failed (2 test cases); reverted, `cmp` byte-identical, `git diff HEAD -- Source` empty.
- Fixed the stale `tests/CMakeLists.txt` comment about the Windows job.
- Counts: ctest Debug 220/220, Release 224/224; `grains_alloc_failure_tests` direct: Debug 2 cases / 5 assertions, Release 6 cases / 224 assertions. /W4 /WX clean. No `armFailure`/`failureFired` in `grains_tests.exe` (Debug, Release).


### PR-B review remediation (elision hazard)
- Finding: the `armFailure` self-tests used `delete new int(i)` / `delete[] new char[8]`. C++14 allows eliding a new/delete pair even with a replaced operator new; clang and gcc do so at -O1+, so an optimized non-MSVC build would see no allocation reach the hook (thrown==0, count()==0). Committed CI (Linux -O0, MSVC) was unaffected.
- Fix (`tests/test_alloc_failure.cpp` only): `observedNewDelete` / `observedNewDeleteArray` publish the pointer through a `void* volatile` sink before deleting, so the allocation is observable and cannot be elided. Assertions unchanged.
- TDD: RED not demonstrable locally. MSVC does not elide, and no clang/gcc/WSL toolchain is installed on this machine (docker-desktop WSL distro only), so the -O2 before/after compile was not run. Justified by the standard rule ([expr.new]) and the volatile store being a required side effect that depends on the pointer.
- Verification: see final report (Debug 220/220, Release 224/224, /W4 /WX clean).
