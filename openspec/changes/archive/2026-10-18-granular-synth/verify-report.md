# Verify Report: granular-synth

Mode: hybrid, Strict TDD. Branch feat/granular-synth-04b-realtime @ 1e58e1e. Runner: ctest (Catch2 v3.8.1), MSVC 19.51, Debug.

## Verdict: PASS WITH WARNINGS (0 CRITICAL, 5 WARNING, 4 SUGGESTION). Ready for sdd-archive.

## Runtime evidence
- Fresh dir `build-verify`: configure OK, build OK (/W4 /WX), `ctest -C Debug`: 100% passed, 78/78 (73 grains_tests + 5 grains_realtime_tests), 7.16 s, exit 0.
- `cmake -S . -B build-plugin-check -DGRAINS_BUILD_PLUGIN=ON` FAILS as required ("GRAINS_BUILD_PLUGIN is not supported yet (roadmap #1)", exit 1). Default configure passes.
- Log: fresh-build ctest output captured locally during verification (not committed).
- Build dirs deleted; git status clean. No coverage tool available (skipped, not a failure).

## Tasks
35/35 checked (PR1 11, PR2 9, PR3 9, PR4 6). Each backed by an apply-progress entry; test files for every task exist and pass.

## Traceability (20 requirements: 7 R-GS, 9 R-GR, 4 R-BH)
Counts: 17 covered, 3 weak, 0 uncovered.

| Req | Covering tests (ctest #) | Status |
|---|---|---|
| R-GS-1 | 29, 30, 31, 32 | covered |
| R-GS-2 | 34 | covered |
| R-GS-3 | 33 | covered |
| R-GS-4 | 35, 37, 38 | covered |
| R-GS-5 | 36, 11, 12 | covered |
| R-GS-6 | 41, 73, 23, 25 | covered (73 asserts activeAtStop == 128 and untouched grains; mutation-checked) |
| R-GS-7 | 39, 2-6, 40 | covered |
| R-GR-1 | 7, 8, 9, 10 | covered; WEAK-minor: no explicit [0,1] range assertion (only within 1e-3 of Hann) |
| R-GR-2 | 47, 48, 49 | covered |
| R-GR-3 | 57, 58, 59, 44, 45, 46 | covered; no sanitizer run, OOB proven by guarded buffers + exact-value assertions |
| R-GR-4 | 61, 62, 51 | covered |
| R-GR-5 | 71, 72, 60, 63 | covered (RMS 3 dB noise+sine, step bound; mutation-checked) |
| R-GR-6 | 64, 65 | covered |
| R-GR-7 | 67, 68, 69, 70, 56, 54, 55 | WEAK: (a) sample-rate scenario asserts finite/bounded/peak>0.01 only, not that grain ms scale to correct sample counts; (b) "clamping equivalence" pitch +100 == +24 proven at params level (56), not as engine output identity; grid itself is strong (300 combos, sounding > 150, mutation-checked) |
| R-GR-8 | 74-78 | covered (full pool REQUIREs saturation; leak mutation fails 4/5) |
| R-GR-9 | 66 | covered |
| R-BH-1 | configure passes without JUCE; deferred JUCE-link guard in Source/core/CMakeLists.txt; harness #1 | covered (non-ctest runtime check) |
| R-BH-2 | 1 + all discovered tests | WEAK: failing-test scenario evidenced only historically (task 1.3 in apply-progress), not reproducible in suite; accepted by design |
| R-BH-3 | git diff vs main on Grains.jucer/Builds/JuceLibraryCode empty | covered (static) |
| R-BH-4 | openspec/config.yaml: runner ctest, layers.unit true | covered (static) |

## Design deviations
| Deviation | Judgment |
|---|---|
| Absolute-time scheduler (double nextTime_ + int64 blockStart_) | Acceptable; strictly stronger block-split guarantee. Design Scheduler paragraph should be amended |
| Render inside scheduler callback (no offset buffer) | Acceptable; matches design's event-ordered segments, avoids unbounded buffer |
| One RNG draw per spawn (also jitter 0) | Acceptable; consistent with R-GS-5/6 |
| Final semitones clamp to +-24 | Acceptable; keeps rate bounded; consistent with R-GR-7 pitch range. Design amendment recommended |
| setSource resets pool | Acceptable (avoids dangling grains on source swap) |
| Null/empty source does not advance scheduler | Acceptable; spec only requires exact 0.0. Note: schedule resumes on source set (documented) |
| activeGrainCount() accessor | Acceptable; read-only, test-support |
| Separate grains_realtime_tests exe | Acceptable; isolates replaced operator new. Matches the expected 73+5 |
| Extremes 0.25 s per combo (spec 1 s) | Acceptable with WARNING: pool saturates ~0.13 s so worst case reached; spec scenario says 1 s |
| 4.4 reformulation (pool-full untouched test; skip-draw proven in scheduler test 41 with desync sensitivity check) | Acceptable |
| PR3a/3b, PR4a/4b splits | Acceptable; driven by 800-line budget, each slice green |
| Idle density-0 time not banked | Acceptable |
No violations; no deviation breaks a spec.

## Known limitations (recorded)
Same set as design.md "Known limitations" and archive-report.md "Known Limitations".
1. Coherent DC (constant-1 source, density 200, 1000 ms, gain 2): pre-clamp peak ~9.3 (gain 1 ~4.6) is hard-clipped at +-4. R-GR-7 compliant (4.0 is the documented ceiling) but audible distortion for DC-like material; test 70 pins it. Soft limiter deferred to the plugin stage.
2. AllocationCounter replaces global operator new/new[] (and aligned forms) only; direct malloc/calloc/platform allocators (or third-party allocators) are invisible.
3. Extremes realtime test (77) does not assert active grains (48 combos could pass with idle engine); saturation is asserted only in test 76.
4. JUCE-free guard checks only link properties of grains_core (deferred recursive check of LINK_LIBRARIES/INTERFACE_LINK_LIBRARIES); does not inspect include paths/sources and was verified by temporary mutation, not by a permanent test.
5. Extremes grid runs 0.25 s per combination vs spec 1 s; the pool saturates at ~0.13 s, so the worst case is reached.
6. R-GR-7 sample-rate grain-length scaling and pitch +100 == +24 engine-level identity are weakly covered (see Traceability).
7. Determinism guaranteed per binary/flags/compiler only (design).
8. No sanitizer/coverage runs.

## Strict TDD compliance
- Evidence table present but uses a RED/GREEN (and mutation) format, no Safety Net / Triangulate columns: WARNING.
- Several tasks (3.7/3.8, 4.1-4.4) passed on first run (characterization tests); compensated by recorded mutation checks: WARNING.
- Assertion audit: no tautologies, no ghost loops found in reviewed tests; sanity guards (CHECK_FALSE(allZero), sounding > 150, REQUIRE saturation) present. Loops over fixed arrays only.
- All tests are unit-layer (78 tests, 16 files); no integration/E2E layers configured.

## Issues
CRITICAL: none.
WARNING:
1. R-GR-7 sample-rate scenario does not assert grain-size scaling; clamping-equivalence only at params level.
2. Extremes grid runs 0.25 s/combo vs spec 1 s (tasks 4.2 also states per-combo 1 s implicitly via spec).
3. Realtime extremes test (77) does not assert active grains.
4. TDD evidence format lacks Safety Net/Triangulate; several tests passed first run (mitigated by mutations).
5. No sanitizer run for engine OOB (R-GR-3 relies on guarded buffers and exact-value tests).
SUGGESTION:
1. Amend design.md: absolute-time scheduler, +-24 st final clamp, setSource semantics, separate realtime exe, activeGrainCount.
2. Add engine-level test pitch 100 == 24 bit-identical and a sample-rate grain-length check; assert [0,1] in window test.
3. Add REQUIRE(activeGrainCount()>0) in the realtime extremes loop for non-zero-density combos.
4. Consider soft-limiter or documented headroom for coherent DC (roadmap), and a CI job with ASan/UBSan.

Ready for sdd-archive: yes.
