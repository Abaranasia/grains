# Verification Report: loader-hardening

Mode: Strict TDD (ctest/Catch2), hybrid persistence. Verdict: PASS WITH WARNINGS (0 CRITICAL, 1 WARNING, 2 SUGGESTION).

## Completeness
Tasks: all automated tasks checked. Pending (user-performed [Manual]): 1.8, 1.9, 4.11. Pending post-push: 6.1. Commit tasks 1.10/2.9/3.4/4.12 and 5.3 updated by verify (3f8f65b, 415a457, 5a732ee, aade368).

## Execution evidence
| Command | Exit | Result |
|---|---|---|
| cmake --build build --config Debug | 0 | OK |
| ctest --test-dir build -C Debug --output-on-failure | 0 | 123/123 pass |
| ctest -R "own generation" --repeat until-fail:5 | 0 | pass x5 |
| cmake --build build-plugin --config Debug --target grains_plugin | 0 | VST3 + Standalone built |
Coverage tool: not available (skipped).

## Spec compliance
| Requirement / scenario | Evidence | Status |
|---|---|---|
| R-SS-5 floor, zero, INT_MAX, DBL_MAX, BadSampleRate | test_sample_builder.cpp "maxSourceFrames floors..." (5 CHECKs), passed | COMPLIANT |
| R-SS-7 stop waits, no force kill | SampleLoader.cpp stopThread(-1) (sole stopThread call); stalled-share scenario [Manual] | STATIC OK, MANUAL PENDING |
| R-SS-9 failed clear keeps source, labels | SampleLoader.cpp L82 "Error: No sample", L99/104/146 "Error: <name>"; [Manual] | STATIC OK, MANUAL PENDING |
| R-BH-3 pin from JucePin.cmake, 8 scenarios | include(JucePin), ${GRAINS_JUCE_SHA}/_SHA256 in CMakeLists.txt; plugin build with pin OK; G1-G5 recorded by apply 4.6-4.8 | COMPLIANT (apply evidence + fresh build) |
| R-BH-5 single pin file, cache key | ci.yml:74 hashFiles('cmake/JucePin.cmake'); literals only in cmake/JucePin.cmake (rg outside openspec/ and build dirs); single-source [Manual] | STATIC OK, MANUAL PENDING |
| R-SS-11 (supporting) stress | commits>0 guard; repeated 5x pass; RED mutation proof recorded in apply | COMPLIANT |

## TDD compliance
Evidence present in apply-progress; test files exist and pass (123/123). S2 is characterization (documented). S-A RED mutation recorded (failed on mismatches after 10 s rather than commits==0; reverted; no `gen + 1` leftover). Assertion quality: no tautologies, no ghost loops; stress test asserts commits>0 and mismatches==0.

## Issues
CRITICAL: none.
WARNING: [Manual] scenarios W2 stalled share (1.8), W-A failed clear (1.9), single-source cache key (4.11) unverified at runtime; user-owned.
SUGGESTION: (1) S-A RED proof differs from the task text (mismatches vs commits==0); harmless. (2) Post-push 6.1 CI observation still pending.
