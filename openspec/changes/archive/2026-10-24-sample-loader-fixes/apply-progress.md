# Apply progress: sample-loader-fixes

Status: all tasks done. Core committed as b4559e0 (1.14), plugin as 4143e53 (2.4). Manual 2.5: all 5 Standalone checks confirmed by the user on 2026-10-24. 3.1: no docs commit.

## TDD evidence
| Task | RED failure mode |
|---|---|
| 1.1 G1 | compile error: grains/GenerationGate.h missing |
| 1.3 G2, G3 | characterization pass after 1.2 |
| 1.4 G7 | TEMPORARY: commitIf ran f without the mutex -> `CHECK(seenInside == old)` failed; reverted |
| 1.5 G5 | same sabotage -> mismatch CHECK failed; reverted |
| 1.6 X1 | compile error: stage/commit/Staged missing |
| 1.8 X2 | TEMPORARY: commit deleted displaced Box -> `destroyedInside == 0`, `live == 2`, `displaced` checks failed; reverted |
| 1.9 X3 | passed with 1.7 implementation (characterization) |
| 1.10 G4, 1.11 G6 | TEMPORARY: commit regardless of commitIf result -> `b.changed`, `b.source == a.source`, destroyed checks failed; reverted |
| 1.12 R1 | TEMPORARY: commit deleted displaced Box -> `frees == 0` failed; reverted |

No sabotage remains (rg confirmed; final suite green).

## Results
- Baseline 111/111; final 122/122 (Debug, zero warnings, build/apply-sl clean rebuild).
- Timing tests (`-R gate`, includes G5/G7): 4 consecutive runs green plus full-suite runs; focused `gate|exchange|realtime` 29/29.
- Plugin (build-plugin grains_plugin Debug): VST3 + Standalone build OK, no C4xxx warnings (only the pre-existing CMAKE_GENERATOR_PLATFORM notice).
- No juce references in Source/core sources (only the existing CMake assertion helper).

## Diff stat
Group 1 (core+tests): SourceExchange.h 50, tests/CMakeLists.txt 1, test_engine_realtime.cpp 30, test_source_exchange.cpp 127 (+ new GenerationGate.h ~50, test_generation_gate.cpp ~85, untracked).
Group 2 (plugin): SampleLoader.cpp 36, SampleLoader.h 8.

## Notes
- 2.3 done within group 1 (SourceExchange.h contract comment) at orchestrator request.
- Deviation (design C1): empty-restore OOM posts "Error: No sample" only if still current.
