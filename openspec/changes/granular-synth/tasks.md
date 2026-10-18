# Tasks: Granular Synth Foundation

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~1200 total (PR1 ~250, PR2 ~380, PR3 ~330, PR4 ~240); every slice under 400 and far below the 800 budget |
| 400-line budget risk | High as a single PR (total > 800); Low-Medium per slice |
| Chained PRs recommended | Yes |
| Suggested split | PR1 harness+window+rng -> PR2 buffer+pool+scheduler -> PR3 interpolator+engine -> PR4 hardening tests |
| Delivery strategy | auto-chain |
| Chain strategy | stacked-to-main (user decision 2026-10-17) |

Decision needed before apply: No
Chained PRs recommended: Yes
Chain strategy: stacked-to-main
400-line budget risk: High

Chain note: every slice builds and passes `ctest` alone. stacked-to-main: PRn targets main after PRn-1 merges. feature-branch-chain: PR1 base = tracker branch `feat/granular-synth`; PR2 base = PR1 branch; PR3 base = PR2 branch; PR4 base = PR3 branch. Pick before opening PR1.

### Suggested Work Units

| Unit | Goal | PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|----|----------------------|-----------------|-------------------|
| 1 | CMake+Catch2+ctest, ParamLimits, WindowTable, Rng | PR1 | `ctest --test-dir build -C Debug -R "window\|rng\|harness"` | `cmake -S . -B build -DGRAINS_BUILD_PLUGIN=ON` must fail; default configure must pass | `CMakeLists.txt`, `cmake/`, `Source/core/`, `tests/`, `.gitignore`, `openspec/config.yaml` |
| 2 | SampleBuffer, GrainPool, GrainScheduler | PR2 | `ctest --test-dir build -C Debug -R "buffer\|pool\|scheduler"` | 10^6-sample drift test (offline) | PR2 files only; PR1 stays green |
| 3 | Interpolator, renderGrain, EngineParams, GranularEngine | PR3 | `ctest --test-dir build -C Debug -R "interp\|grain_render\|params\|engine_bounds"` | Engine renders ramp/sine buffers offline | PR3 files only |
| 4 | Determinism, extremes, RMS, isolation, zero-alloc | PR4 | `ctest --test-dir build -C Debug -R "determinism\|extremes\|realtime"` | AllocationCounter around `process()` | `tests/` additions plus engine fixes, if any |

## Spec/Design reconciliation

| # | Mismatch | Resolution in tasks |
|---|----------|---------------------|
| 1 | Density: spec [0,2000], D=0 means silence; design 0.1-1000 | Clamp to [0,1000]; 0 (and negatives, NaN-to-default aside) means no spawns; (0,0.1) clamps to 0.1. Amend R-GS-7 (amended). D2 RESOLVED: cap 1000 |
| 2 | Size: spec [1 sample, 2000 ms]; design 1-1000 ms, grain length >= 2 samples | Follow design. Amend R-GR-7 text (amended). D2 RESOLVED |
| 3 | Pitch, pitch jitter, gain, position, pan ranges | Identical, no action |
| 4 | Output ceiling 4.0 and denormals: spec only | Add final output pass: flush |y|<1e-20 to 0, clamp to +-4.0. Worst-case test in PR4 |
| 5 | Scheduler clamping (R-GS-7) tested on scheduler, design clamps only in `setParams` | `ParamLimits.h` (PR1) holds constants and sanitizers; both `configure()` and `setParams()` use them. NaN to default (density 20, jitter 0) |
| 6 | Block-split: spec bit-identical or 1e-6; design bit-identical | Test requires bit-identical (design). 1e-6 fallback unused |
| 7 | Normalization: spec smoothed active-count gain; design fixed per grain `1/sqrt(max(1,D*size))` | Follow design (no zipper, deterministic). Amend R-GR-5 (amended): gain from params, constant per grain; continuity scenario becomes "density sweep produces no sample step above the documented bound". `gain` param also baked in at spawn |
| 8 | Boundary: spec allows shrink or clamp-start; design shrinks length, moves start back only if len < 2, else skip | Implement design. D1 RESOLVED: shrink. R-GR-3 amended to state shrink explicitly |
| 9 | R-GR-3 scenario "final sample <= 1e-3" fails when a shrunk grain is only a few samples (window phase n/len) | Test asserts tail <= sin^2(pi/len)+eps and no step above window slope bound; amend scenario (amended) |
| 10 | Skipped spawns must still consume RNG draws; design silent | Engine always draws position, pitch, pan (fixed order, even at jitter 0) before pool/bounds checks; skip occurs after draws. Scheduler draws its interval at spawn. Test in PR2/PR3 |
| 11 | R-GR-2 unity scenario (`window*source`) vs pan centre gain 0.707 | Free function `renderGrain<Interp>` in `Grain.h`; tests set `gainL=gainR=1`. Engine keeps constant-power pan |
| 12 | First spawn time unspecified | First spawn at sample 0 after `reset()` (gives exactly 100 for D=100, 48000 samples) |
| 13 | Position jitter semantics unspecified | `start = clamp01(position + jitter*bipolar())*(frames-1)` |
| 14 | Sample-rate range unclamped | `prepare()` clamps sr to [8000, 384000] |
| 15 | R-BH-4 | PR1 updates `openspec/config.yaml` as in design |

User decisions (2026-10-17), all RESOLVED:
- D1 RESOLVED: grains SHRINK near the buffer end (design default). Spec boundary scenarios state shrink explicitly.
- D2 RESOLVED: keep design caps: density [0, 1000] gr/s (0 = no spawns), grain size 1-1000 ms, minimum grain length 2 samples. R-GS-7 and R-GR-7 amended.
- Roadmap (out of scope): selectable playback modes (forward, wrap/loop, reverse, random position, etc.) recorded in proposal.md roadmap item 8.

## PR1: Harness, limits, window, rng

- [x] 1.1 RED: `tests/CMakeLists.txt` (Catch2 v3.8.1 FetchContent, `catch_discover_tests`), `tests/test_harness.cpp`; configure fails (no root CMake)
- [x] 1.2 GREEN: root `CMakeLists.txt` (3.25, C++17, `GRAINS_BUILD_PLUGIN=OFF`, ON = FATAL_ERROR), `cmake/GrainsWarnings.cmake`, `.gitignore` `build/`; ctest green (R-BH-1, R-BH-2)
- [x] 1.3 Verify R-BH-2: temporary `REQUIRE(false)` makes ctest non-zero and names the case; revert
- [x] 1.4 Add configure-time check in `Source/core/CMakeLists.txt`: fail if `grains_core` links any `juce*` target
- [x] 1.5 RED: `tests/test_params_limits.cpp` (clamp ranges, NaN to default, density 0 off, sr clamp)
- [x] 1.6 GREEN: `Source/core/include/grains/ParamLimits.h`; create `grains_core` STATIC
- [x] 1.7 RED: `tests/test_window.cpp` (R-GR-1: endpoints 0, centre ~1, symmetry, 1000 random phases within 1e-3 of Hann)
- [x] 1.8 GREEN: `WindowTable.{h,cpp}` (4096-entry Hann, interpolated `at`)
- [x] 1.9 RED: `tests/test_rng.cpp` (same seed identical, different seed differs, `uniform01` in [0,1), `bipolar` in [-1,1], reseed)
- [x] 1.10 GREEN: `Rng.h` (PCG32, no globals)
- [x] 1.11 Update `openspec/config.yaml`: `runner: ctest`, `layers.unit: true` (R-BH-4); REFACTOR warnings clean under /W4 /WX

## PR2: SampleBuffer, pool, scheduler

- [x] 2.1 RED: `tests/test_sample_buffer.cpp` (mono/stereo accessors, immutable, empty, 1-frame)
- [x] 2.2 GREEN: `SampleBuffer.{h,cpp}`
- [x] 2.3 RED: `tests/test_pool.cpp` (capacity 128, acquire returns nullptr when full, release reuse, index-order `forEachActive`, reset)
- [x] 2.4 GREEN: `Grain.h` (struct only), `GrainPool.h`
- [x] 2.5 RED: `tests/test_scheduler.cpp`: periodic 100 gr/s gives 100 spawns 480 apart; D=70 over 10 s within +-1 sample of k*fs/D; D=0 none; offsets in [0,B); first spawn at 0 (R-GS-1, R-GS-3)
- [x] 2.6 RED: same file: block-split invariance (1, 64, 1-sample, irregular); jitter 0 ignores seed; same seed identical, seeds 1/2 differ; jitter=1 mean within 5% over 20 seeds x 60 s; monotonic; clamp extremes D=-5/1e9/NaN, jitter -1/7 (R-GS-2, 4, 5, 7)
- [x] 2.7 GREEN: `GrainScheduler.{h,cpp}` (double accumulator, `T*(1+j*bipolar())` floored at 1, floor offset, uses ParamLimits)
- [x] 2.8 RED+GREEN: skip-on-full schedule test: capacity 128 vs unlimited gives identical due-times; freed slot is reused (R-GS-6)
- [x] 2.9 REFACTOR: shared test helpers in `tests/support/`

## PR3: Interpolator, grain render, engine

- [x] 3.1 RED: `tests/test_interpolator.cpp` (exact at integers, midpoints, last-sample read stays in range, no OOB under guarded buffer)
- [x] 3.2 GREEN: `Interpolator.h` (`LinearInterpolator`, `InterpolationMode`, index clamp guard)
- [x] 3.3 RED: `tests/test_grain_render.cpp`: unity pitch equals `window(n/256)*src[1000+n]` within 1e-6; +12 st reads `2n` within 1e-5; start unchanged by pitch (R-GR-2)
- [x] 3.4 GREEN: `renderGrain<Interp>` in `Grain.h`
- [x] 3.5 RED: `tests/test_params.cpp` (EngineParams clamp table incl. NaN, pitch 100 equals 24)
- [x] 3.6 GREEN: `EngineParams.h`, clamp in `setParams`
- [x] 3.7 RED: `tests/test_engine_bounds.cpp`: near-end grain shrinks, no OOB, no step above window slope bound; tiny 1/2-sample source finite; density 0 or null source gives exact 0.0; mono equals dual-mono; spread 0 centres (R-GR-3, 4, 5)
- [x] 3.8 GREEN: `GranularEngine.{h,cpp}` (segment loop, spawn bounds, fixed 3 draws before skip, pan, norm gain, flush+ceiling pass)
- [x] 3.9 REFACTOR: constants to named values, document ceiling and gain bound

## PR4: Hardening and RT safety

- [x] 4.1 RED+GREEN: `tests/test_engine_determinism.cpp`: same seed bit-identical (2 s); block split 512/37/1/one block bit-identical; interleaved instances A/B equal solo runs (R-GR-6, 9)
- [x] 4.2 RED+GREEN: `tests/test_engine_extremes.cpp`: grid size/pitch/density/position per R-GR-7, finite and |y|<=4.0; sample rates 22050-192000; denormal-free silence; worst-case coherent DC at full pool vs ceiling
- [x] 4.3 RED+GREEN: RMS test, noise source, 50 ms, 20 vs 400 gr/s within 3 dB; density-sweep step bound (amended R-GR-5)
- [x] 4.4 RED+GREEN: skip-draw test: saturated pool leaves active grain output identical to unsaturated first 128 (R-GS-6)
- [ ] 4.5 RED: `tests/support/AllocationCounter.{h,cpp}` (replace `new/new[]/aligned`, RAII scope) and `tests/test_engine_realtime.cpp`: 0 allocations normal, pool-full, extremes, block sizes up to 4096 (R-GR-8)
- [ ] 4.6 GREEN: fix any engine allocation or ceiling breach found; final `ctest` green on a fresh build dir
