# Apply progress: loader-hardening

Mode: Strict TDD. Status: 41/48 tasks done in the working tree (uncommitted). Pending: manual 1.8, 1.9, 4.11; orchestrator commits 1.10, 2.9, 3.4, 4.12; 5.3 (needs commits); post-push 6.1.
Final: core ctest 123/123 (baseline 122 + 1 S2 case); plugin build (build-plugin and a fresh fetch build) succeeded.

## Per-WU files
- WU1: Source/plugin/SampleLoader.cpp, Source/plugin/SampleLoader.h
- WU2: tests/test_generation_gate.cpp, tests/test_engine_realtime.cpp, tests/test_sample_builder.cpp
- WU3: Source/core/include/grains/SourceExchange.h, SampleBuilder.h
- WU4: cmake/JucePin.cmake (new), CMakeLists.txt, .github/workflows/ci.yml

## TDD Cycle Evidence
| Task | RED | GREEN | REFACTOR |
|---|---|---|---|
| WU1 W2 stop(-1) | no deterministic test (stalled share is [Manual]); callers verified: only ~GrainsAudioProcessor and ~SampleLoader | ctest 122/122, plugin built | n/a |
| WU2 S-A | mutation commitIf(gen + 1) on the new test: FAILED after ~10 s deadline (`mismatches == 0` got 36), no hang | unmutated: pass, --repeat until-fail:5 pass | mutation reverted from backup, no `gen + 1` left |
| WU2 S1 | no deterministic RED (optimizer dependent) | 123/123 | n/a |
| WU2 S2 | characterization, passed on first run | 123/123 | S4 cmath removed, builds |
| WU3 S3/S5 | comments only | build + 123/123 | n/a |
| WU4 V1/V5 | RED: literal SHA matched in CMakeLists.txt (L38, L43) and ci.yml L74 | G1 fresh fetch configure + grains_plugin build OK; G3 bad digit -> "Hash mismatch", reverted; G2 FATAL_ERROR, no juce _deps; G4 local JUCE; G5 core -Werror=dev -Werror=deprecated OK; SHA now only in cmake/JucePin.cmake | n/a |

## Work Unit Evidence
| WU | Focused test | Runtime harness | Rollback boundary |
|---|---|---|---|
| 1 | ctest 122/122 | plugin build OK; stalled share [Manual] | SampleLoader.cpp/.h |
| 2 | `ctest -R contention --repeat until-fail:5` pass; full 123/123 | ctest | the three test files |
| 3 | build + 123/123 | N/A (comments) | the two headers |
| 4 | configure-level G1-G5 above | fresh fetch build | JucePin.cmake, CMakeLists.txt, ci.yml |

## Deviations
- S-A RED failed on `mismatches` (after the 10 s deadline), not `commits == 0` as the design predicted: the mutated committer succeeds once bumps start. Still fails without hanging.
- The RED mutation sed also touched line 27 (another test) during the experiment; the file was restored from a backup of the GREEN version.
- 2.8 UBSan: N/A on MSVC.
