# Apply Progress: granular-synth-followups

Mode: Strict TDD (characterization tests; RED shown by temporary sabotage, reverted, `git diff -- Source` empty at the end).
Delivery: single PR, two planned commits (`test:` T1-T7, `ci:` T8-T11). Nothing committed, staged or pushed by apply.
Status: 10/11 tasks complete. T9 pending (needs the first CI run).

Baseline before changes: 78/78 (MSVC Debug). Final: 84/84 Debug, 84/84 Release.

## TDD Cycle Evidence

| Task | Test file | Layer | Safety net | RED (sabotage, then observed failure) | GREEN | Triangulate | Refactor |
|---|---|---|---|---|---|---|---|
| T1 | `tests/test_engine_bounds.cpp` | Unit | 78/78 | A: `+ 0.5` -> `+ 0.0` in `requested`: `220 == 221` at 22.05 kHz. B: length from `srcSr`: `441 == 480` (48 kHz engine, 44.1 kHz source) | Passes restored, incl. the 22.05 kHz tie (221) | 6 cases (5 rates + rate mismatch) | None needed |
| T2 | `tests/test_engine_extremes.cpp` | Unit | 78/78 | Removing only the `sanitizePitchSemitones` clamp did NOT fail (spawn() clamps again, redundant by design). Removing BOTH clamps: `upHigh.L == up.L` fails | Passes restored | +-100 vs +-24 plus 23 control (both signs) | None needed |
| T3 | `tests/test_engine_extremes.cpp` | Unit | 78/78 | `clampTo` removed in `spawn()`: `1063 >= 2000` (+24, seed 3) | Passes restored; seeds 1..16 hit both exact bounds (2000 and 32000), no widening needed | +24 and -24 | None needed |
| T4 | `tests/test_window.cpp` | Unit | 78/78 | `at()` result `* 1.0001f`: `103437 == 104098` (values above 1) | Passes restored | boundary phases, dense sweep, nextafter(1,0); peak > 0.99 guard | None needed |
| T5 | `tests/test_engine_realtime.cpp` | Unit (alloc counter) | 78/78 | A: `spawn()` returns early: `0.0f > 0.001f`, `0 == 128`, `0 > 0` (45 assertions failed). B: scheduler config forced to density >= 1 when density 0: `peak == 0.0f` fails (0.13..0.70 observed) | Passes restored | density 0 vs >0, saturation at both pitches | None needed |
| T6 | `tests/test_engine_extremes.cpp` | Unit | 78/78 | Ceiling clamp removed in `finalize()`: `4.043085098f <= 4.0f` (pitch=-100, position=-1) | Passes restored | 4 raw-value combos | Grid comment updated |
| T8 | `CMakeLists.txt` | Config | 84/84 | Pre-change tree: `-DGRAINS_SANITIZE=ON` only warns "not used by the project", no error | MSVC + ON fails configure with the designed FATAL_ERROR; default OFF configures, builds, 84/84 | N/A (structural) | None needed |
| T10 | `.github/workflows/ci.yml` | Config | N/A (new) | Absence of any workflow before the change | YAML parses (PyYAML); keys, jobs, permissions, concurrency checked | N/A (structural) | None needed |

## Work Unit Evidence

| Evidence | Value |
|---|---|
| Focused test commands | `grains_tests.exe "[engine_bounds]"`, `"[engine_extremes]"`, `"[window]"`, `grains_realtime_tests.exe`; all green after each GREEN |
| Runtime harness | Full `ctest --test-dir build -C Debug --output-on-failure`: 84/84 (78 baseline + 6 new test cases). Release: 84/84. Linux clang ASan/UBSan path and the workflow itself cannot run locally; verified by the first CI run |
| Rollback boundary | Commit 1: the four `tests/*.cpp` files. Commit 2: `CMakeLists.txt` and `.github/workflows/ci.yml` |

## New tests

T1 grain length table, T2 pitch +-100 equals +-24, T3 spawn-side clamp, T4 window range sweep, T5 saturation test (plus extended extremes asserts and `RunStats`), T6 1 s raw worst-case subset. T5 extends existing cases, so the delta is +6 test cases (78 -> 84).

## T10 pin

`actions/checkout` pinned to the full SHA of `v5.1.0` (also what the moving `v5` tag pointed to at apply time), resolved with `git ls-remote https://github.com/actions/checkout 'refs/tags/v5*'`. The tag is lightweight, so there is no `^{}` entry: the listed SHA is the commit. `v6` tags exist upstream; the design asked for v5, so v5 was kept.

## Deviations and findings

- T2 sabotage: the pitch clamp exists in two places (`setParams` and `spawn()`), so removing one is not detectable by an output test. The test turns RED only when both are removed. This is defense in depth, not a gap; T3 covers the spawn side separately.
- T5: `maxActive > 0` is asserted per combination only for size 1000 ms with position 0. 1 ms grains and the 2-sample move-back grains (position 1) are released inside the block, so a per-block poll cannot see them; their audibility is covered by `peak > kSoundThreshold`.
- T5 sabotage on the scheduler density guard (`density_ >= 0`) hung the test (infinite period); it was killed and reverted, and a different density-0 sabotage was used for evidence.
- No engine fix was needed. `Source/` is untouched.

## Remaining

- T9 (CI-dependent): after the user pushes, watch the first run. Clang `-Werror` budget 0-20 lines; `float-cast-overflow` fix up to 30 lines, otherwise drop the flag and update the R-BH-5 delta spec and design flag list.
