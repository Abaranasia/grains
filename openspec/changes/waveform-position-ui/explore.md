# Exploration: waveform-position-ui (roadmap #2)

Mirror of engram `sdd/waveform-position-ui/explore`.

## Current State
- Editor (`Source/plugin/PluginEditor.{h,cpp}`): `GrainsEditor` embeds `juce::GenericAudioProcessorEditor` + Load button + status label (40px bar); `FileDragAndDropTarget`; `ChangeListener` on `proc.statusBroadcaster()`. No custom paint, no Timer.
- Processor: APVTS "GrainsState", 9 float params from core `grains::params::kSpecs`; `position` is {0..1, skew 1.0}, so normalised == plain and linear. `positionJitter` is also 0..1.
- Sample path: `SampleLoader` (juce::Thread) decodes -> `grains::SampleBuilder` -> `SampleBuffer` -> `SourceExchange<SampleBuffer>` (SPSC; the audio thread is the only consumer). The editor/message thread has NO access to the `SampleBuffer` by design (reading `current_` from the UI would race with retire/free). `SampleLoader::Result` is posted on the message thread via `onResult_` -> `GrainsAudioProcessor::onLoadResult` -> `status_` + ChangeBroadcaster. A Reload on host rate change republishes a resampled buffer (generation gate).
- Engine: `GranularEngine::spawn` start = `clamp01(position + jitter*bipolar) * (frames-1)`; `GrainPool` has 128 fixed slots; only introspection today is `activeGrainCount()`.
- Core is JUCE-free (CMake assertion R-BH-1). Tests are Catch2 in `grains_tests`, `grains_realtime_tests`, `grains_alloc_failure_tests`.
- Specs: plugin-shell R-PS-6 says editor = generic editor + loader UI and forbids other custom parameter UI; Out-of-scope lists waveform display, so a MODIFIED delta is needed. R-PS-3: exactly 9 params. sample-source R-SS-1..3 unchanged. grain-rendering R-GR-6/R-GR-8: any snapshot hook must not change audio, RNG draws or allocate.

## Affected Areas
- `Source/plugin/PluginEditor.{h,cpp}`: new layout + WaveformView component (Timer ~30 Hz).
- `Source/plugin/PluginProcessor.{h,cpp}`: hold peaks, expose grain snapshot reader; publish snapshot after `engine_.process`.
- `Source/plugin/SampleLoader.{h,cpp}`: compute peaks on the loader thread, attach `shared_ptr<const WaveformPeaks>` to `Result`.
- `Source/core` (new): `WaveformPeaks`, `WaveformGeometry`, `GrainSnapshot` (+ `GranularEngine::snapshotGrains() const noexcept`); CMake registration for sources and tests.
- openspec: plugin-shell (R-PS-6 modify), new capability e.g. `waveform-display`.

## Key design findings
1. Waveform thread-safety: compute peaks on the loader thread (pure core function) and deliver as an immutable `shared_ptr` inside `Result` on the message thread. No locks, no change to `SourceExchange`. Mirror the `status_` generation filtering; clear on Restore clear or missing file.
2. Position control: attach drag/click to the existing `position` via `juce::ParameterAttachment` with change gestures. Draw jitter band `[pos-jit, pos+jit]` clamped to [0,1]. Marker follows host automation.
3. Active grains: `snapshotGrains(GrainSnapshot&) const noexcept` writes up to 128 normalised positions + count into a fixed POD. Publish via `array<atomic<float>,128>` + `atomic<int>` with relaxed stores once per block; tearing is harmless for display. Must not alter RNG/audio.
4. Strict TDD: core Catch2 for peaks, geometry, snapshot (including bit-identical render with/without hook, zero-alloc in realtime tests). Manual-only: paint, Timer, mouse gestures, automation display, resize/DPI, editor open/close during load, repaint CPU.
5. Glue gotchas: editor sizing from generic editor; Timer/ChangeListener lifecycle; peaks live in the processor so a late-opened editor can pull them.

## Approaches
| | Approach | Pros | Cons | Effort |
|---|---|---|---|---|
| A | Native JUCE component (peaks via Result, atomic grain snapshot, Timer) | Fits thin-glue model, no deps, logic testable in core | Native look limited; later superseded by #5 (core pieces reusable) | Medium, ~500-700 lines |
| B | Waveform + position drag only, grains later | Lowest risk, no engine change | No live grains | Low-Medium |
| C | Build directly on WebView (#5) | One UI | Huge scope, blocks #2 on #5 | High |
| D | UI reads refcounted buffer copy | Simple UI side | Breaks R-SS-2/3 ownership; rejected | Medium |

## Recommendation
Approach A, one PR if under 800 lines, otherwise split: (1) core peaks + geometry + loader peaks + static waveform + position drag; (2) GrainSnapshot + live grain markers.

## Roadmap relations
- #5 WebView: peaks, snapshot POD and geometry are bridge-friendly; the native view is an interim step and fallback.
- #8 playback modes: snapshot should carry direction/rate sign and tolerate out-of-range positions; keep position mapping in core.

## Risks
- Spec conflict (R-PS-6, out-of-scope line) needs a delta.
- Generic editor is fixed size; enlarging may need a resizable window.
- Peaks are normalised-x, so rate reloads must redeliver or keep them.
- Timer repaint cost with many instances; repaint only on change.
- Almost all UI scenarios are manual.

## Open product questions
1. Position selection: click/drag only, or also drag-a-range for jitter width?
2. Live active-grain display in this change or a follow-up?
3. Stereo display: mixed mono, stacked L/R, or overlay?
4. Zoom/scroll or full sample only?
5. Keep generic sliders below the waveform? Resizable window?
6. Interim native UI vs wait for #5?
7. Playhead for future scan/loop modes (#8) now?
8. Colours/theme constraints?

## Ready for Proposal
Yes, after the user answers questions 1, 2 and 6.
