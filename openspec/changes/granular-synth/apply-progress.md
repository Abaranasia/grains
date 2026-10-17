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
