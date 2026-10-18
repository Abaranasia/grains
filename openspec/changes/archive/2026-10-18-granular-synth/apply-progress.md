# Apply Progress: granular-synth

## PR1 / Work Unit 1 (branch feat/granular-synth-01-harness): DONE (tasks 1.1-1.11, 11/11)

### TDD evidence
| Task | RED | GREEN |
|------|-----|-------|
| 1.1/1.2 | `cmake -S . -B build` -> "does not appear to contain CMakeLists.txt"; tests/ alone fails (no project()) | root CMake + warnings; 1/1 ctest passed |
| 1.3 | temporary `REQUIRE(false)` -> ctest "1 - harness: ... (Failed)", non-zero | reverted, 100% passed |
| 1.4 | n/a (config-time check); verified by temp `juce_fake` link -> FATAL_ERROR "grains_core must not link JUCE targets" | reverted; configure passes |
| 1.5/1.6 | build error C1083: grains/ParamLimits.h not found | 6/6 passed |
| 1.7/1.8 | build error C1083: grains/WindowTable.h not found | 10/10 passed |
| 1.9/1.10 | build error C1083: grains/Rng.h not found | 16/16 passed (incl. PCG32 reference vector 0xa15c02b7) |
| 1.11 | config.yaml: runner ctest, layers.unit true | `ctest -R "window|rng|harness"` 11/11; build clean under /W4 /WX |

`-DGRAINS_BUILD_PLUGIN=ON` configure fails with FATAL_ERROR (verified).

### Files
Modified: .gitignore (+build/), openspec/config.yaml, openspec/changes/granular-synth/tasks.md.
New: CMakeLists.txt, cmake/GrainsWarnings.cmake, Source/core/CMakeLists.txt,
Source/core/include/grains/{ParamLimits,WindowTable,Rng}.h, Source/core/src/{ParamLimits,WindowTable}.cpp,
tests/CMakeLists.txt, tests/test_{harness,params_limits,window,rng}.cpp.
Changed lines: ~16+/14- tracked, ~426 untracked (total ~455).

### Deviations
- Task order: grains_core CMake (1.4) created together with 1.6 since the target does not exist earlier.
- ParamLimits.cpp added (include-only) so grains_core STATIC has a source until WindowTable.cpp; kept as self-containment check.
- ParamLimits API: namespace grains::limits, sanitizeDensity/SizeMs/Unit/PitchSemitones/PitchJitterSemitones/Gain/SampleRate plus k* constants.
- Window: table[i]=Hann(i/(kSize-1)); at() interpolates over kSize-1 intervals, out-of-range/NaN -> 0.
- Rng seeds like pcg32_srandom_r(seed, stream 54).
- Test names use "group: description" prefix so ctest -R works.

## PR2 / Work Unit 2 (branch feat/granular-synth-02-pool-scheduler): DONE (tasks 2.1-2.9, 9/9)

### TDD evidence
| Task | RED | GREEN |
|------|-----|-------|
| 2.1/2.2 | build error C1083: grains/SampleBuffer.h not found | 22/22 passed (6 buffer cases) |
| 2.3/2.4 | build error C1083: grains/GrainPool.h not found | 28/28 passed (6 pool cases) |
| 2.5/2.6/2.7 | build error C1083: grains/GrainScheduler.h not found (2.5 and 2.6 tests written together, one RED) | 41/41 passed |
| 2.8 | schedule test passed on first run (scheduler is pool-independent by construction); asserts due-time equality, max active 128, no skip while not full, slot reuse (accepted > 128) | 41/41 |
| 2.9 | moved Spawn/run helper to tests/support/SchedulerHelpers.h | 41/41; focused `-R "buffer|pool|scheduler"` 25/25 |

Fresh build dir: clean under /W4 /WX; full ctest 41/41.

### PR1 follow-ups included
1. JUCE-free guard: deferred (`cmake_language(DEFER)`) recursive check of LINK_LIBRARIES and INTERFACE_LINK_LIBRARIES; verified with a temp `target_link_libraries(grains_core INTERFACE juce_fake)` added after the subdirectory -> FATAL_ERROR; reverted.
2. Catch2 pinned to commit 2b60af89e23d28eefc081bc930831ee9d45ea58b (v3.8.1 peeled tag); GIT_SHALLOW dropped (unreliable with SHA); offline via FIND_PACKAGE_ARGS 3 (-DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=ALWAYS) or FETCHCONTENT_SOURCE_DIR_CATCH2.
3. WindowTable.h comment corrected: out-of-range/NaN returns 0 (not clamped).

### Files
New: Source/core/include/grains/{SampleBuffer,Grain,GrainPool,GrainScheduler}.h, Source/core/src/{SampleBuffer,GrainScheduler}.cpp,
tests/test_{sample_buffer,pool,scheduler}.cpp, tests/support/SchedulerHelpers.h.
Modified: Source/core/CMakeLists.txt, tests/CMakeLists.txt, WindowTable.h, tasks.md, apply-progress.md.

### Deviations
- Scheduler tracks an absolute double time (`nextTime_`) plus int64 `blockStart_` instead of design's per-block `samplesUntilNext`: guarantees bit-identical spawn times for any split (relative accumulation rounds differently per split). Offset = floor(nextTime_) - blockStart_.
- Rng drawn once per spawn always (also at jitter 0) for fixed consumption; design said "only at spawn", consistent.
- Idle (density 0) time is not banked: on re-enabling, next spawn is clamped to the current block start (no burst).
- SampleBuffer: mismatched right channel size is ignored (mono); empty left gives 0 channels.
- GrainPool: acquire resets the slot to a default Grain with active=true; lowest-index free slot.
- Task 2.8 test used a simulation with GrainPool; Grain::pos/length used as start/length bookkeeping in the test only.

## PR3a / Work Unit 3, part 1 (branch feat/granular-synth-03a-render): DONE (tasks 3.1-3.6, 6/9) + 3 PR2 follow-ups

PR3 exceeded the 800-line review budget (~1040 lines) and was split: PR3a (follow-ups, interpolator, renderGrain, EngineParams) and PR3b (GranularEngine, tasks 3.7-3.9).

### PR2 review follow-ups (done first)
1. Skip-on-full test rewritten (tests/test_scheduler.cpp): scheduler shares its Rng with a callback that always takes one draw; a run with a real 128 GrainPool (acquire/release by end time) is compared with a pool-free run. Asserts equal due-times, maxActive == 128, skipped > 0, accepted > 128, no skip while not full. Sensitivity check inside the test: a callback that skips its draw on a full pool must produce different due-times (so the test would catch a desynchronised stream). No Grain::pos misuse (end times kept in the test).
2. tests/CMakeLists.txt: FIND_PACKAGE_ARGS removed; pinned SHA fetch is the default; installed Catch2 only via explicit `-DGRAINS_USE_SYSTEM_CATCH2=ON`; offline via FETCHCONTENT_SOURCE_DIR_CATCH2; `catch2_SOURCE_DIR` guarded. Verified: with FETCHCONTENT_TRY_FIND_PACKAGE_MODE=ALWAYS Catch2_DIR is the FetchContent redirect, not a system install; ON with none installed fails configure.
3. 'tiny interval floors at one sample' now asserts strictly increasing times and min gap >= 1. RED verified: removing the `scaled < 1.0 ? 1.0 : scaled` floor failed it; floor restored.

### TDD evidence
| Task | RED | GREEN |
|------|-----|-------|
| 3.1/3.2 | C1083 grains/Interpolator.h not found | 5/5 |
| 3.3/3.4 | C2039/C2065 renderGrain not a member of grains | 5/5 (10/10 with interp) |
| 3.5/3.6 | C1083 grains/EngineParams.h not found | 5/5 |

Fresh incremental build clean under /W4 /WX.

### Files
New: Source/core/include/grains/{Interpolator,EngineParams}.h, tests/test_{interpolator,grain_render,params}.cpp.
Modified: Grain.h (renderGrain), tests/CMakeLists.txt, tests/test_scheduler.cpp, tasks.md, apply-progress.md.

### Deviations / decisions
- renderGrain takes srcL and srcR; srcR == srcL (mono) reads once and feeds both channels, giving bit-identical mono vs dual-mono.
- Grain pos accumulates `pos += rate`; startOffset field unused.
- EngineParams::clamped() holds the clamping (3.6); the engine setParams call site lands in PR3b.

## PR3b / Work Unit 3, part 2 (branch feat/granular-synth-03b-engine): DONE (tasks 3.7-3.9, 9/9 for WU3)

### TDD evidence
| Task | RED | GREEN |
|------|-----|-------|
| 3.7/3.8 | C1083 grains/GranularEngine.h not found | 7/7 passed FIRST run after the engine existed (no failing run on the implementation); mutation check: replacing the shrink with `len = requested` fails near-end and normalization tests, then reverted |
| 3.9 | named constants (kOutputCeiling, kFlushThreshold, kMinGrainLength) and bound docs in GranularEngine.h | full 63/63 |

### Files
New: Source/core/include/grains/GranularEngine.h, Source/core/src/GranularEngine.cpp, tests/test_engine_bounds.cpp.
Modified: Source/core/CMakeLists.txt, tests/CMakeLists.txt, tasks.md, apply-progress.md.

### Deviations / decisions
- Segment loop interleaves rendering inside the scheduler callback (no offset buffer): spawns per block are unbounded (up to 1/sample), so a fixed buffer could overflow.
- Final semitones (pitch + jitter*draw) clamped to +-24 so rate stays bounded.
- setSource resets the pool; no source / empty source => exact zeros and the scheduler is not advanced.
- setParams calls EngineParams::clamped() and reconfigures the scheduler.
- OOB is checked by guarded-buffer interpolator tests and finite/bounded engine output; no sanitizer run.

## PR4a / Work Unit 4, part 1 (branch feat/granular-synth-04a-hardening): DONE (tasks 4.1-4.4, 4/6) + 4 PR3b follow-ups

PR4 hit the ~700-line cap before 4.5 and was split: PR4a (follow-ups, tasks 4.1-4.4) and PR4b (AllocationCounter + realtime zero-alloc tests, tasks 4.5-4.6).

Stopped at the ~700 changed-line cap (about 713 lines of tests/support/doc before 4.5). 4.5 (AllocationCounter + realtime tests) and the fix half of 4.6 remain; the fresh-build full ctest half of 4.6 was run (73/73) but 4.6 stays open until 4.5 lands.

### PR3b review follow-ups (done first)
1. GranularEngine.h ceiling doc corrected: the clamp engages for coherent (DC-like) material. Measured pre-clamp peak: constant-1 source, density 200, size 1000 ms, gain 2 -> about 9.3 (gain 1 -> about 4.6; gain 0.5 stays below 4). No gain change; the +-4 clamp is the R-GR-7 documented bound, so no spec violation.
2. New test "move-back fallback renders a 2-sample grain inside the source" (position 1.0, ramp source, rates 1 and 2: only L[1] non-zero = src[start+rate]*cos(pi/4)). Mutation checks: fallback start kept at `last`, fallback length 3, and removing the `start < 0` skip all fail the tests; reverted.
3. "tiny sources" test is now discriminating: 1-frame silent; 2 frames silent at rate 1 and sound at rate 0.25 (-24 st); 3 frames sound at rate 1 and silent at rate 4 (+24 st). Sounding cases assert the exact value 0.8*cos(pi/4).
4. `spawn(int)` unused parameter removed (header, definition, call site). Test kPi and helpers moved to tests/support/EngineHelpers.h (single `grains_test::kPi`).

### TDD evidence
| Task | Tests | RED/GREEN | Mutation check |
|------|-------|-----------|----------------|
| 4.1 | test_engine_determinism.cpp (3) | characterization: passed first run | extra block-dependent RNG draw fails block-split; shared static state fails all three |
| 4.2 | test_engine_extremes.cpp (4) | passed first run (grid uses a mostly-DC source so the clamp matters; 0.25 s per combination instead of 1 s for Debug speed) | removing the +-4 clamp fails grid and DC test; removing the flush fails the denormal test |
| 4.3 | test_engine_loudness.cpp (2); RMS test moved here from bounds (now 5 s, noise+sine, 2 seeds, stereo) | passed first run | no normalization fails RMS; active-count-dependent render gain fails RMS and the sweep step bound |
| 4.4 | test_engine_pool_full.cpp (1, two configs) | passed after fixing the test (first version used a short source so grains shrank and freed slots) | kCapacity 256 fails |

### Files
New: tests/support/EngineHelpers.h, tests/test_engine_{determinism,extremes,loudness,pool_full}.cpp.
Modified: GranularEngine.h/.cpp, tests/CMakeLists.txt, tests/test_engine_bounds.cpp, tasks.md, apply-progress.md.

### Remaining
- 4.5: tests/support/AllocationCounter.{h,cpp} (scoped, atomic counter active only inside an RAII scope; replacement of new/new[]/aligned and matching deletes) and tests/test_engine_realtime.cpp. Plan: separate executable `grains_realtime_tests` so the replacement operators do not affect the shared test binary.
- 4.6: fix any allocation found, then final fresh-build ctest.

## PR4b / Work Unit 4, part 2 (branch feat/granular-synth-04b-realtime): DONE (tasks 4.5-4.6; all 6/6 of PR4, change complete) + 1 PR4a follow-up

### Implementation
- tests/support/AllocationCounter.{h,cpp}: replaces every global operator new/new[] (plain, nothrow, align_val_t, aligned nothrow) and all matching deletes (plain, sized, nothrow, aligned, sized-aligned). Plain pair uses malloc/free; aligned pair uses _aligned_malloc/_aligned_free on MSVC (aligned_alloc/free elsewhere). Counting only while an RAII AllocationScope is alive (atomic flag + counter); deletes are never counted.
- Separate executable `grains_realtime_tests` (tests/CMakeLists.txt, catch_discover_tests) so the replaced operators never touch `grains_tests`.
- tests/test_engine_realtime.cpp (5 tests, prefixed "realtime:"): counter self-test (counts inside scope incl. over-aligned new, zero outside), normal settings, full pool (warm-up outside scope, REQUIRE saturated), parameter-extreme grid (48 combinations), block sizes 1/37/512/4096. Setup, setParams and buffer sizing are outside the scope (R-GR-8 constrains render calls only).
- GranularEngine::activeGrainCount() const noexcept: read-only accessor over the pool counter, added for saturation assertions.

### PR4a follow-up
tests/test_engine_pool_full.cpp: removed hand-derived stopAt (6400/10000); now derived from GrainPool::kCapacity, period kSr/density and a per-case margin (1.05 / 1.6), rounded up to a block. Added REQUIRE(stopAt < window) and CHECK(activeAtStop == kCapacity). Mutation: engine density 200 instead of the 1000 used for derivation fails with 28 == 128; reverted.

### TDD evidence
- RED: realtime tests failed to compile (activeGrainCount missing) after the counter and tests existed; GREEN after the accessor: 5/5 pass.
- Mutation: `std::vector<float> leak(n)` in GranularEngine::process fails 4 of 5 realtime tests (counter self-test still passes); reverted.
- 4.6: no allocation found in the engine; no engine fix needed. Fresh build ctest: see final report.
