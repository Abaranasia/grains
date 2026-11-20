# Apply Progress: load-time-resampling

Mode: Strict TDD. Delivery: single PR, `size:exception` (user-approved, about 960 changed lines, no chain split).
Status: 20/21 tasks complete. Task 4.6 (manual host checks R-SS-15, R-PS-8) is pending for the user.

## Completed

All of 1.1-1.2, 2.1-2.8, 3.1-3.3, 4.1-4.5, 5.1-5.2. See `tasks.md` for the checkmarks.

## TDD Cycle Evidence

| Task | Test file | Layer | Safety net | RED | GREEN | TRIANGULATE | REFACTOR |
|------|-----------|-------|------------|-----|-------|-------------|----------|
| 1.1 | existing tests | Unit | 173/173 | n/a (move) | 173/173 after move | n/a | Clean |
| 1.2 | `tests/test_resampler.cpp` | Unit | n/a (new) | Stub build | Builds | n/a (structural) | Clean |
| 2.1 | `tests/test_resampler.cpp` | Unit | n/a (new) | Run: 3 cases fail (stub) | n/a | n/a | n/a |
| 2.2 | `tests/test_resampler.cpp` | Unit | n/a (new) | see 2.1 | 3/3 pass (71 assertions) | 14 invalid-arg rows + valid control, mono+stereo | Clean |
| 2.3 | `tests/test_resampler.cpp` | Unit | 3/3 | Run: ripple and impulse fail on zero output | see 2.5 | 2 rate pairs x 7 tones; down and up impulse | n/a |
| 2.4 | `tests/test_resampler.cpp` | Unit | 3/3 | Run: stopband fails on its passband control, upsample image fails | see 2.5 | 2 pairs x 4 tones; 2 upsample cases | n/a |
| 2.5 | `tests/test_resampler.cpp` | Unit | 3/3 | 2.3 and 2.4 (4 test cases red) | 7/7 pass (166 assertions), first try | covered by 2.3/2.4 | `long` to `ptrdiff_t`, decimation limit guard |
| 2.6 | `tests/test_resampler.cpp` | Unit | 7/7 | Written after 2.5, GREEN on arrival (behaviour came with 2.5) | 11/11 pass | 3 inputs x 2 pairs; 3 chunk sizes; non-identical stereo control | Clean |
| 2.7 | `tests/test_resampler.cpp` | Unit | 7/7 | GREEN on arrival (as 2.6) | 13/13 pass (230 assertions) | abort on 3rd/1st poll, never-abort (5 polls), cap exact/cut/equal-rate, ratio limit | Clean |
| 2.8 | `tests/test_resampler.cpp` | Perf (hidden) | n/a | n/a | Release: 2.41 s resample wall time (limit 3 s) | n/a | n/a |
| 3.1 | `tests/test_sample_builder.cpp` | Unit | 5/5 builder | Compile error (5-arg overload missing) | see 3.2 | 5 new cases | n/a |
| 3.2 | `tests/test_sample_builder.cpp` | Unit | 5/5 | 3.1 | 10/10 builder pass (105 assertions) | resample, equal rate, caps (input/exact/output cut), 5 invalid targets, abort | Shared `validate()` extracted |
| 3.3 | `tests/test_grain_render.cpp` | Unit/engine | 5/5 | GREEN on arrival (engine unchanged by design) | 7/7 pass | native vs 96k vs builder-resampled; negative control with mis-tagged rate | Clean |
| 4.1-4.5 | none possible in CI | n/a | n/a | n/a | `GrainsPlugin_Standalone` Debug builds, no warnings | n/a | n/a |

### Notes on evidence
- 2.6, 2.7 and 3.3 were GREEN on arrival: the production behaviour already existed (2.5 implemented the full renderer; the engine needs no change per R-GR-7). They are characterization tests with controls (non-trivial inputs, negative controls), not strict RED-first.
- The R-GR-7 "bit-identical to baseline" check compares the builder passthrough against a plainly constructed buffer (and a mis-tagged rate as negative control) instead of a golden hash. A golden would depend on libm bits across MSVC and clang.
- `OutOfMemory` (task 2.7) is handled but untested: no allocation-failure injection exists in the harness.
- Phase 4 has no automated tests (JUCE code is not built in CI). It was compile-checked with the local `build-plugin` tree (`cmake --build build-plugin --config Debug --target GrainsPlugin_Standalone`).

### Test Summary
- New test cases: 20 (13 resampler visible + 1 hidden perf, 5 builder, 2 engine). ctest: 193/193 pass in Debug and Release (baseline was 173/173, not 78).
- Layers: Unit 20, Perf 1 (hidden), Integration/E2E 0.

## Work Unit Evidence

| Evidence | Unit 1 (core) | Unit 2 (loader and processor) |
|---|---|---|
| Focused test command | `ctest --test-dir build -C Debug -R "resample|builder|render"` and `grains_tests "[resampler]"`: all pass | `cmake --build build-plugin --config Debug --target GrainsPlugin_Standalone`: builds |
| Runtime harness | `grains_tests.exe "resample: 300 s stereo 96k to 48k within 3 s"` Release: 2.41 s, pass | Manual only: R-SS-15, R-PS-8 in a host (pending for user) |
| Rollback boundary | `Resampler.{h,cpp}`, `ChannelView.h`, `SampleBuilder.{h,cpp}`, CMake lists, three test files | `SampleLoader.{h,cpp}`, `PluginProcessor.{h,cpp}` |

## Deviations from design
1. `Request` carries a `label` (Reload only) instead of the loader reading `published_->label` at failure time. Equivalent (a newer commit would make the Reload non-current and silent), and avoids taking `lock_` in `load`.
2. `PluginProcessor` gains `std::atomic<uint64_t> restoreGeneration_`, set after the restore request in `setStateInformation`. `onLoadResult` persists a `loaded && User` result before the generation filter only when `r.generation > restoreGeneration_`. Without it, a User result queued before a state restore would overwrite the restored path properties.
3. `buildTable` refuses ratios beyond 1000:1 (`InvalidArgument` from `resample`, `BadSampleRate` from the builder) so a pathological ratio cannot request a huge kernel. Not in the design; the design's 384k to 8k case (48:1) is unaffected.
4. Table taps: `2K+1` per row as designed; `retarget` also ignores non-finite or non-positive rates.

## Issues / risks
- Perf margin is thin: 2.41 s of 3 s on this machine (Release, double accumulation, not vectorised). A slower reference machine could fail the hidden check; it is not in CI.
- R-SS-15 and R-PS-8 are untested beyond compilation. Pending manual checks: rate change reload (96k file, 44.1k to 48k), restore before prepare (file-rate publish then one reload; equal rates no reload), file deleted on reload (buffer stays, label not "Missing"), rapid rate changes (only the last published), superseded long resample aborts.
- The old "78/78" baseline in the artifacts was stale; the real baseline is 173.
