# Apply progress: loader-followups (PR-A, Phases 1-3)

Mode: Strict TDD. Status: PR-A complete (12/12 tasks); PR-B (Phases 4-6) not started.

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
