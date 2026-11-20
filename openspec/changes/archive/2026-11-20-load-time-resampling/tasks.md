# Tasks: Load-time resampling (P3)

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~710 code+tests, ~250 spec deltas: ~960 total |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | PR 1 (core, ~600) -> PR 2 (plugin, ~110) |
| Delivery strategy | single-pr (review_budget_lines=800) |
| Chain strategy | pending |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: pending
400-line budget risk: High

Forecast exceeds the 800-line budget if spec deltas count; code alone (~710) fits. Single-pr needs `size:exception` or a split.

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | Core resampler + builder overload | PR 1 | `ctest --test-dir build -C Debug -R "resample|builder|render" --output-on-failure` | `[.][perf]` Release run of `resample: 300 s stereo 96k to 48k within 3 s` | Resampler files, builder overload, tests |
| 2 | Loader target/retarget/Reload + processor wiring | PR 2 (base = PR 1 branch) | full ctest (plugin is JUCE, not CI-built) | Manual R-SS-15, R-PS-8 in host | SampleLoader.*, PluginProcessor.cpp |

Test commands: `cmake -S . -B build -A x64 -DGRAINS_BUILD_PLUGIN=OFF`; `cmake --build build --config Debug`; `ctest --test-dir build -C Debug --output-on-failure` (baseline 78/78).

## Phase 1: Foundation (PR 1)

- [x] 1.1 Move `ChannelView` to `Source/core/include/grains/ChannelView.h`; `SampleBuilder.h` includes it. Existing tests stay green.
- [x] 1.2 Create `Resampler.h` (status enum, options, result, `resampledFrames`) with a stub `Resampler.cpp`; register in `Source/core/CMakeLists.txt` and `tests/CMakeLists.txt` (add `test_resampler.cpp`).

## Phase 2: Core resampler, test-first (PR 1)

- [x] 2.1 RED `tests/test_resampler.cpp`: length table, 1-frame case (R-RS-1), invalid args (R-RS-8), bit-exact passthrough mono/stereo (R-RS-2).
- [x] 2.2 GREEN: `resampledFrames`, argument validation, passthrough copy.
- [x] 2.3 RED: passband ripple 0.1 dB and phase 1e-4 rad, 96k->44.1k and 44.1k->96k; impulse placement (R-RS-3).
- [x] 2.4 RED: stopband tones 1.1/1.3/1.6/1.9 x dst/2 at -100 dB; upsample image residual at -100 dB (use design tones, not the 0.55 x src typo).
- [x] 2.5 GREEN: Bessel I0, Kaiser table (beta 11.16, P=ceil(1024 fc)), row normalisation, double 4-way accumulation, edge skipping, `t=m*ratio` render.
- [x] 2.6 RED+GREEN: finite edge inputs 22050<->192000 (R-RS-4); determinism, chunk 65536 vs 777 (R-RS-5); identical stereo channels (R-RS-7).
- [x] 2.7 RED+GREEN: abort on 3rd poll, at most 2 chunks rendered, no channels; never-abort completes (R-RS-6); `OutOfMemory` and `maxFrames` capped flag.
- [x] 2.8 Add hidden `[.][perf]` test (R-RS-9); run manually in Release.

## Phase 3: Builder and engine scenarios (PR 1)

- [x] 3.1 RED `tests/test_sample_builder.cpp`: resample to 48000 gives 48000 frames; equal rate bit-exact; output cap at 192000 target; invalid target (0, -1, NaN, Inf) -> `BadSampleRate` (R-SS-5).
- [x] 3.2 GREEN: `BuildError::Aborted` and 5-arg `buildSampleBuffer` in `SampleBuilder.{h,cpp}`; input cap at file rate, output cap at target rate.
- [x] 3.3 RED/GREEN `tests/test_grain_render.cpp`: mismatched 96k source keeps 1 kHz within 1 percent; matched rate bit-identical to baseline (R-GR-7). No engine change expected.

## Phase 4: Loader and processor (PR 2)

- [x] 4.1 `SampleLoader.h`: `Request.targetRate`, `Origin::Reload`, `target_`, `latest_`, `settled_`, `published_`, `bool retarget(double)`.
- [x] 4.2 `SampleLoader.cpp`: `request()` copies `target_`; `load` calls the target overload with `shouldAbort=!isCurrent()`, silent on `Aborted`, frees decode before staging.
- [x] 4.3 Implement `retarget` under `lock_`: no-op if equal; reissue `latest_` if in flight; else Reload from `published_` when rate differs. Set `published_` after `commitGated`, reset on clear.
- [x] 4.4 Reload: no "Loading:" post; on failure keep buffer and repost `published_->label`, never "Missing".
- [x] 4.5 `PluginProcessor.cpp`: `loader_.retarget(sampleRate)` after `engine_.prepare`; in `onLoadResult` write the four APVTS properties for `loaded && User` before the generation filter.
- 4.6 Moved to "Deferred follow-ups" below (user decision: archive with manual host checks outstanding).

## Phase 5: Cleanup

- [x] 5.1 Run the full ctest; confirm the 78/78 baseline plus new cases; MSVC /W4 /WX clean.
- [x] 5.2 Resolve the R-RS-3 "0.55 x src" scenario text (0.275 x src) when archiving specs.

## Deferred follow-ups

Not completed at archive time (user decision). These need a real plugin host; the logic has no automated tests (verify W2).

- D.1 R-SS-15: 96 kHz file with host going 44.1 kHz to 48 kHz: exactly one reload, published rate becomes 48000 Hz.
- D.2 R-PS-8: restore before the first `prepareToPlay`: file plays at its own rate, then one reload; equal rates cause no reload.
- D.3 File deleted when a rate-change reload runs: old buffer keeps playing and the label does not read "Missing".
- D.4 Rapid rate changes: only the last result is published.
- D.5 A long resample superseded by a newer load aborts and publishes nothing.
