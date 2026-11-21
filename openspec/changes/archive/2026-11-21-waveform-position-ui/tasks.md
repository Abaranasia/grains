# Tasks: Waveform and Position UI (roadmap #2)

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~600 code+tests, ~195 spec deltas, ~60 change docs: 800-900 total |
| Review budget | 800 lines (additions + deletions) |
| Budget risk | High (at or slightly over budget; not hidden) |
| Chained PRs recommended | Yes (optional; fallback if apply runs over) |
| Suggested split | PR 1 core + tests + specs (~480) -> PR 2 plugin glue (~320) |
| Delivery strategy | single-pr |
| Chain strategy | size-exception (single PR needs maintainer size:exception) |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: size-exception
400-line budget risk: High

Note: budget is 800 here, not the default 400. Forecast is 800-900, so single-pr requires `size:exception` unless the split is chosen.

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | Core peaks, geometry, state + tests + specs | PR 1 | `ctest -R "waveform"` on `grains_tests` (+ alloc sweep target) | N/A: JUCE-free, host-free | Core headers/sources, tests, CMake lists |
| 2 | Loader/processor wiring, view, editor | PR 2 (base = PR 1 branch) | Full `grains_tests` + Release build of plugin | Standalone + DAW manual checklist (Phase 6) | Plugin files; core stays unused |

## Phase 1: WaveformGeometry (R-WD-2)

- [x] 1.1 RED: `tests/test_waveform_geometry.cpp` (round trip, clamp, zero width, band clamp, non-finite); register in `tests/CMakeLists.txt`; must fail to compile/run.
- [x] 1.2 GREEN: `Source/core/include/grains/WaveformGeometry.h` header-only `xToPosition`, `positionToX`, `Band jitterBand`.

## Phase 2: WaveformPeaks, columnPeak (R-WD-1)

- [x] 2.1 RED: `tests/test_waveform_peaks.cpp` (mixed mono, 10-bucket spike, empty, F<N, NaN/Inf, determinism, mono identity, `columnPeak` coverage and width 0).
- [x] 2.2 GREEN: `Source/core/include/grains/WaveformPeaks.h` + `src/WaveformPeaks.cpp` (`PeakBucket`, `makeWaveformPeaks` noexcept, 64-bit bucket ranges, `columnPeak`); register in `Source/core/CMakeLists.txt`.
- [x] 2.3 RED then GREEN: `tests/test_alloc_failure.cpp` sweep (behind `GRAINS_ALLOC_SWEEPS`): nth-allocation failure returns null, never throws.

## Phase 3: WaveformState (R-WD-3 policy, D4/D5)

- [x] 3.1 RED: tests in `test_waveform_peaks.cpp`: loaded sets peaks, cleared empties, failed later load keeps peaks, null peaks on loaded gives NoWaveform, `apply` returns changed flag, commit-order supersede.
- [x] 3.2 GREEN: `WaveformState` + `WaveformDisplay` in `WaveformPeaks.h/.cpp`.
- [x] 3.3 Gate: `grains_tests` and R-BH-1 JUCE-free check green. Optional PR 1 boundary.

## Phase 4: Loader and processor wiring (R-WD-3)

- [x] 4.1 `Source/plugin/SampleLoader.h/.cpp`: add `peaks`, `cleared` to `Result`; compute after `isCurrent()`, before `exchange_.stage`; `cleared` only on accepted clear commit; publish even if peaks null; redeliver on Reload (R-PS-8).
- [x] 4.2 `Source/plugin/PluginProcessor.h/.cpp`: `WaveformState waveform_`, `waveform()`, `parameter(id)` accessor; apply in `onLoadResult` in commit order before the label generation filter; broadcast on change.
- [x] 4.3 Verify `SourceExchange`, `processBlock`, engine untouched (diff check).

## Phase 5: WaveformView and editor (R-WD-4, R-PS-6)

- [x] 5.1 Create `Source/plugin/WaveformView.h/.cpp`: paint columns via `columnPeak`, marker, jitter band, hints (Empty/NoWaveform); no Timer.
- [x] 5.2 Mouse down/drag via `xToPosition` into `ParameterAttachment(position)` with begin/end gesture; `positionJitter` attachment callback-only repaint.
- [x] 5.3 `Source/plugin/PluginEditor.h/.cpp`: add view above generic editor (bar 40 + view 140 + generic), `setResizable(false, false)`, pull `proc_.waveform()` in constructor, update in `changeListenerCallback`.
- [x] 5.4 Register `WaveformView.cpp` in `Source/plugin/CMakeLists.txt`; Release build clean (Windows).

## Phase 6: Manual verification (Standalone and DAW)

Run in the Standalone (build-plugin Release `Grains.exe`) and in a DAW with the VST3. Use a short WAV with a clear transient plus a long one. Tick each sub-item as it passes.

- [ ] 6.1 R-WD-3 scenarios
  - [ ] Load: Load... a WAV; the waveform appears and matches the file.
  - [ ] Stale load: request a large file then immediately a small one; only the small one's waveform stays.
  - [ ] Failed later load: with A shown, load a corrupt/renamed non-audio `.wav`; label shows Error, A still plays and its waveform stays.
  - [ ] Missing-file restore: save state with a sample, delete/rename the file, reload state; waveform clears, "Drop a sample" shows, label reads "Missing: <name>".
  - [ ] Rate change: switch the audio device sample rate; after the reload the waveform is still shown and matches.
  - [ ] Peaks failure: not reproducible without fault injection; covered by the core alloc sweep (`test_alloc_failure`). Code path: null peaks gives "No waveform" with audio unaffected.
  - [ ] Late-opened editor: load a sample, close the editor, reopen it; the waveform is shown.
  - [ ] Bit-identical audio: audio path files untouched (`git diff` shows no change to SourceExchange, GranularEngine, processBlock); optionally compare a rendered bounce before/after.
- [ ] 6.2 R-WD-4 scenarios
  - [ ] Click jump: click at ~75% width; `position` becomes ~0.75 and the marker jumps there.
  - [ ] Drag clamp: press then drag past both edges; `position` clamps to 0 and 1.
  - [ ] Automation: in a DAW with write mode on, press/drag/release; one gesture is recorded on the `position` lane.
  - [ ] Jitter band: drag the Position Jitter slider; the band width follows. Dragging in the view never changes `positionJitter`.
  - [ ] Marker follows automation: play back `position` automation; the marker moves with it.
  - [ ] Empty hint: fresh instance shows "Drop a sample".
  - [ ] Fixed size: the window cannot be resized; no Timer in the view (`rg Timer Source/plugin` finds nothing).
  - [ ] Editor open during load: with the editor open, load a file and restore state; the view updates without a crash.
- [ ] 6.3 R-PS-6: the 9 generic parameter controls, the Load button and status label are all still present and working.

Spec sync not needed until archive.
