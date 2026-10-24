# Tasks: granular-synth-followups

Delivery: single PR, two commits. Commit 1 `test(engine): ...` (T1-T7). Commit 2 `ci: ...` (T8-T11).
Strict TDD: characterization tests prove existing behavior, so RED is obtained by a TEMPORARY sabotage of the engine (or of the assertion) that is reverted and NEVER committed. After each RED/GREEN pair run `git diff` to confirm no sabotage remains.

Local verification: Windows MSVC build + ctest. CI (Linux clang ASan/UBSan, workflow syntax, checkout SHA) can only be truly verified after the user pushes the branch and the workflow runs.

Local verification command (all test tasks):
`cmake --build <build-dir> --config Debug` then `ctest --test-dir <build-dir> -C Debug --output-on-failure`

## Commit 1: test(engine)

### [x] T1 (a) Grain length table, R-GR-7
- Files: `tests/test_engine_bounds.cpp`
- RED: add a table test with literal lengths for 10 ms at 22050/44100/48000/96000/192000 Hz -> 221/441/480/960/1920 (22.05 kHz is a .5 tie), plus a case where source rate != engine rate keeps 480. Sabotage: change the rounding/length formula in `GranularEngine.cpp` (e.g. truncate instead of round); confirm the test fails; revert.
- GREEN: with the engine restored the test passes without engine changes.
- Verify: ctest filter on the bounds suite, then full ctest.

### [x] T2 (b1) Engine pitch clamp via setParams, R-GR-7
- Files: `tests/test_engine_extremes.cpp`
- RED: add test that pitch +/-100 produces output bit-identical to +/-24, with control that 24 vs 23 differ. Sabotage: remove the pitch clamp in `setParams`; confirm failure; revert.
- GREEN: restored engine passes.
- Verify: ctest filter on the extremes suite.

### [x] T3 (b2) Spawn-side clamp test, R-GR-7
- Files: `tests/test_engine_extremes.cpp`
- Setup (per design): constant-1 source, 8001 frames, density 0.1, size 1000 ms, position 0, pitch jitter 12, seeds 1..16.
- Assert: at +24 all grain len >= 2000 and some seed has exactly 2000; at -24 all len <= 32000 and some seed has exactly 32000.
- RED: sabotage by removing `clampTo` around `semis` in `spawn()`; confirm failure; revert.
- GREEN: restored engine passes. If no seed hits both draw signs, widen the seed range (design caveat).
- Verify: ctest filter on the extremes suite.

### [x] T4 (d) Window range sweep, R-GR-1
- Files: `tests/test_window.cpp`
- Test: boundary phases k/(kSize-1) for every k, plus i/100000 for i = 0..100000, plus `nextafter(1,0)`. Assert isfinite and value in [0,1].
- RED: sabotage the window table (e.g. perturb an endpoint/overshoot a value) to push a value out of [0,1] or non-finite; confirm failure; revert.
- GREEN: restored passes.
- Verify: ctest filter on window suite.

### [x] T5 (c) Realtime extremes asserts, R-GR-8
- Files: `tests/test_engine_realtime.cpp`
- Add `RunStats{allocs, maxActive, peak}` observed after each block; update existing callers to use `.allocs`.
- Density 0: maxActive == 0 and peak == 0.0f exactly. Density > 0: peak > kSoundThreshold (1e-3f). Saturation (size 1000, density 1000, jitter 0, position 0): maxActive == kCapacity. Extremes run asserts maxActive > 0 and audible output.
- RED: sabotage the scheduler (e.g. spawn when density == 0, or never spawn) and confirm the relevant assertions fail; revert.
- GREEN: restored passes. Test names must keep the `realtime:` prefix (needed by the ctest -E fallback in T10).
- Verify: ctest filter `-R "^realtime:"`.

### [x] T6 (f) 1 s worst-case extremes subset + grid comment, R-GR-7
- Files: `tests/test_engine_extremes.cpp`
- 4 combos with RAW grid values: size 1e9 x density 1e9 x pitch {-100,+100} x position {-1,2}, max jitters, gain 2, 48000 samples, block 1000. Assert finite output, bounded values, no crash. Keep the main grid at >= 0.25 s. Update the grid comment.
- RED: sabotage the sanitize-then-cast ordering or output bounding (e.g. remove a clamp in `setParams`); confirm failure; revert. If MSVC does not trap the sabotage, a NaN/Inf assertion failure is sufficient evidence.
- GREEN: restored passes.
- Verify: ctest filter on extremes suite. Note: this is the scenario float-cast-overflow targets under clang UBSan (see T9).

### [x] T7 Commit 1 gate
- Apply status: gate verified (84/84 Debug and Release, only test files changed); the commit itself is left to the orchestrator.
- Run full ctest (MSVC Debug, then Release if cheap). Confirm `git diff` contains only test changes (plus any engine fix from T9, which then belongs in this commit). Commit as `test(engine): ...` with conventional message, no AI attribution.

## Commit 2: ci

### [x] T8 GRAINS_SANITIZE option
- Files: `CMakeLists.txt` (root)
- Place after `include(GrainsWarnings)` and before `add_subdirectory`. `option(GRAINS_SANITIZE ... OFF)`. If ON: FATAL_ERROR under MSVC; otherwise directory-scoped `add_compile_options` and `add_link_options` with `-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all -fno-omit-frame-pointer`.
- RED: configure with `-DGRAINS_SANITIZE=ON` under MSVC and expect the FATAL_ERROR message (cannot be exercised before the option exists; sabotage = run on the pre-change tree and observe no error).
- GREEN: MSVC + ON fails with a clear message; MSVC + default (OFF) configures and builds unchanged; full ctest still passes.
- Verify locally: both configure invocations. The clang flags path cannot be run on Windows MSVC; it is verified in CI.

### [x] T9 Clang -Werror fix budget and float-cast-overflow cap (CI-dependent)
- DONE 2026-10-24 with no source change: the first CI run on c2dfc74 passed both jobs (Windows MSVC; Linux clang ASan + UBSan with float-cast-overflow and `-fno-sanitize-recover=all`). Zero clang `-Werror` fixes needed, no UBSan report, flag set kept as designed.
- Original plan (kept for reference): not done at apply time, only actionable after the first CI run. Budget: 0-20 lines of source change for clang `-Werror` warnings (fix, do not suppress broadly).
- float-cast-overflow: if UBSan reports a real overflow, fix in the engine only if the fix is <= 30 lines; the fix goes with a regression assertion in the T6/T3 tests.
- Drop-flag path: if the fix exceeds 30 lines, drop `float-cast-overflow` from the `GRAINS_SANITIZE` flags (keep address,undefined), record the finding as a follow-up, and:
  - update the R-BH-5 delta spec and design flag list to match;
  - make sure the archive/spec merge reflects the final flag set (the main `build-harness` spec must not claim float-cast-overflow);
  - note the dropped coverage in the archive report.
- Because the user pushes branches, if the fix is needed it lands as a follow-up commit on the same PR; keep the two-commit layout by squashing/fixup before merge if the user prefers.

### [x] T10 CI workflow, R-BH-5
- Files: `.github/workflows/ci.yml`
- Resolve the `actions/checkout` v5 full commit SHA AT APPLY TIME (query the upstream tag, e.g. `git ls-remote https://github.com/actions/checkout refs/tags/v5*` and dereference the annotated tag `^{}`); pin `uses: actions/checkout@<sha> # v5`. Never use a floating tag or a SHA recalled from memory.
- Triggers: push to `main` + every `pull_request` (no branch filter). `permissions: contents: read`. `concurrency` group cancels superseded runs. `timeout-minutes: 30`.
- Linux job: ubuntu-24.04, clang++-18, Ninja, Debug, `sudo sysctl vm.mmap_rnd_bits=28`, configure `-DGRAINS_SANITIZE=ON`, build, `ctest --output-on-failure`.
- Windows job: windows-latest, `-A x64`, Debug, `ctest -C Debug --output-on-failure`.
- ASan/realtime conflict fallback: if the allocation-counter `realtime:` tests conflict with ASan, use `ctest -E "^realtime:"` in the LINUX job only (Windows still runs them). This relies on Catch2 test names keeping the `realtime:` prefix (T5).
- RED: no workflow on the tree before the change (absence is the RED). Validate YAML locally with a parser/actionlint if available.
- GREEN: YAML parses; MSVC local build + ctest equals what the Windows job runs. Real green requires a push.
- Required status check is out of scope.

### [x] T11 Commit 2 gate
- Apply status: gate verified (only `CMakeLists.txt` and `.github/workflows/ci.yml`); the commit itself is left to the orchestrator.
- Confirm `git diff` shows only `CMakeLists.txt` and `.github/workflows/ci.yml` (plus any T9 change if folded in). Commit as `ci: ...`. After the user pushes, watch the first run; handle T9 outcomes.

## Parallelism
- T1-T6 are independent files except T2/T3/T6 (same file, edit sequentially). T1, T4, T5 and the T2/T3/T6 chain can be done in any order. T7 follows all.
- T8 and T10 are independent in content but T10 depends on T8's option name; T9 depends on the first CI run after push; T11 last.

## Spec traceability
- R-GR-1: T4. R-GR-7: T1, T2, T3, T6. R-GR-8: T5. R-BH-5: T8, T9, T10.

## Review Workload Forecast
- Estimated changed lines: about 330-430 (tests ~250-300, CMake ~15, workflow ~70, OpenSpec docs excluded; optional T9 fix up to 20-50).
- 400-line budget risk: Medium (against the 400-line guideline; low against the session's 800-line review_budget_lines).
- Chained PRs recommended: No. Single PR with two commits is within the 800-line budget and the commits split cleanly for review.
- Decision needed before apply: No. Only conditional decision is T9 (drop flag vs fix), which is pre-decided by the 30-line cap.
