# Verify Report: granular-synth-followups

Branch feat/granular-synth-followups, HEAD a72f639. Strict TDD, MSVC Debug.

## Verdict: PASS (0 CRITICAL, 0 WARNING, 2 SUGGESTION)

## Execution
- Fresh configure + build in build/verify-followups (gitignored), `ctest -C Debug`: 84/84 passed (7.94 s).
- Working tree clean apart from untracked build-plugin/ (pre-existing, unrelated). No source edited.
- CI (reported by orchestrator via GitHub API, commit c2dfc74): "Windows MSVC" success, "Linux clang ASan + UBSan" success.

## Requirement matrix
| Req | Evidence | Status |
|---|---|---|
| R-GR-1 window shape | test_window.cpp: endpoints/centre, symmetry, 1000 random phases within 1e-3 | OK |
| R-GR-1 dense sweep | test_window.cpp:53-70, i/100000 for i in 0..100000 + k/(N-1) boundaries + nextafter(1,0), finite and in [0,1] | OK |
| R-GR-7 extremes grid (>=0.25 s) | test_engine_extremes.cpp:43 (size, pitch, density, position grid, 12000 frames) | OK |
| R-GR-7 worst-case 1 s subset | test_engine_extremes.cpp:171, raw values, 48000 samples | OK |
| R-GR-7 clamp equivalence, engine level both directions + negative control | test_engine_extremes.cpp:92 | OK |
| Spawn-side clamp (pitch+jitter) | test_engine_extremes.cpp:129, lengths 2000 / 32000, seeds 1..16 | OK |
| Sample-rate range | test_engine_extremes.cpp:207 | OK |
| Grain length per fs (221/441/480/960/1920) + source-rate independence | test_engine_bounds.cpp:144-153 (incl. 48000 engine / 44100 source -> 480) | OK |
| Float-cast overflow under UBSan | ci.yml configures -DGRAINS_SANITIZE=ON; flags include float-cast-overflow + -fno-sanitize-recover=all; CI green | OK |
| R-GR-8 extremes exercise real activity | test_engine_realtime.cpp:133-168 (maxActive > 0, peak > 1e-3, allocs 0) | OK |
| R-GR-8 density 0 | realtime.cpp:162 maxActive == 0, peak == 0 exact | OK |
| R-GR-8 pool full | realtime.cpp:114, 176-193 (maxActive == kCapacity) | OK |
| R-BH-5 workflow triggers | push to main + pull_request without branch filter | OK |
| R-BH-5 hardening | permissions contents: read; concurrency cancel-in-progress; checkout pinned by SHA; timeout 30 | OK |
| R-BH-5 jobs | ubuntu-24.04 clang-18 Ninja Debug + sanitizers; windows-latest x64 Debug; both run ctest | OK |
| R-BH-5 opt-in option | CMakeLists.txt:16-24, default OFF, FATAL_ERROR on MSVC, directory-scoped so Catch2 included, no suppression files | OK |
| R-BH-5 default build unchanged / JUCE-free | default configure+build+ctest green here | OK |
| R-BH-5 realtime exclusion fallback (MAY) | Not needed; sanitizer job runs the full suite | OK |

## Tasks
T1-T11 all [x] in tasks.md; checklist matches headings; T9 closed with no source change (5fd4bbb) because first CI run was clean. Spec traceability consistent.

## Issues
CRITICAL: none.
WARNING: none.
SUGGESTION:
1. The tasks.md forecast mentions "Release if cheap"; only Debug was re-run here (Release was verified at apply, 84/84).
2. Required status check for main is out of scope; consider enabling later so CI actually gates merges.
