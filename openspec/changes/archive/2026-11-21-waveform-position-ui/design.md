# Design: Waveform and Position UI (roadmap #2)

## Technical Approach

Exploration approach A. All math lives in JUCE-free `grains_core` (peaks, column folding, geometry, display-state policy) and is Catch2-tested. `SampleLoader` computes peaks on its thread from the built `SampleBuffer` just before staging, and ships an immutable `shared_ptr<const WaveformPeaks>` in `Result`. The processor adopts the peaks on the message thread. `WaveformView` only paints and maps the mouse through the geometry. `SourceExchange`, `processBlock` and the engine stay unchanged (R-WD-3). Specs: `waveform-display` R-WD-1..4, `plugin-shell` R-PS-6 delta.

## Architecture Decisions

| # | Decision | Rejected alternatives | Rationale |
|---|----------|----------------------|-----------|
| D1 | Fixed bucket count (`kDefaultPeakBuckets = 2048`), independent of view width. The view folds buckets into pixel columns with `columnPeak` | Recomputing per view width; storing all samples | One pass per load. Resizing or DPI changes need no reload. The data stays small (16 KB) and easy to serialise for WebView (#5) |
| D2 | Compute from the built (resampled) `SampleBuffer` after `isCurrent()` and before `exchange_.stage` | Computing from the decode buffer; computing after the commit | After the commit the buffer belongs to the audio thread. Positions are normalised, so the resampled data shows the same shape. A Reload recomputes peaks for free and redelivers them (R-PS-8) |
| D3 | `makeWaveformPeaks` is `noexcept` and returns `nullptr` when allocation fails. Empty input gives a non-null object with no buckets | Throwing; using `BuildError` | Matches the core's no-throw style (`buildSampleBuffer`). If peaks fail, the publish goes ahead anyway |
| D4 | `Result` gains `peaks` and `cleared` (true only when a clear commit is accepted) | Working out "cleared" from `text`/`origin` | A Restore "Error:" (Box allocation failed) keeps the previous source. Only an accepted commit tells the truth |
| D5 | The processor applies `loaded`/`cleared` results **in commit order, before** the label generation filter, through core `WaveformState::apply` | Applying peaks only to results that pass `generation >= latest` | Results are posted in commit order on one thread, so a superseded result can never overwrite newer peaks. Filtering would leave stale peaks when a newer User load fails and the earlier source keeps playing. The view always matches what is audible |
| D6 | Reuse the existing `statusBroadcaster()`. The editor pulls `status()` and `waveform()` | A second broadcaster; a Timer | One notification path. No Timer, as the spec requires |
| D7 | `juce::ParameterAttachment` on `position` (writable) and on `positionJitter` (callback only, never sets) | A hidden `Slider` with `SliderParameterAttachment`; polling the raw atomics | Gives the gesture API, and callbacks arrive on the message thread for automation. Skew is 1, so plain == normalised |
| D8 | Fixed-size editor: bar 40 + view 140 + generic; `setResizable(false, false)` | Resizable or viewport | Product decision. GenericAudioProcessorEditor keeps its own size |

## Data Flow

    loader thread: decode -> buildSampleBuffer -> makeWaveformPeaks -> stage/commitGated
                                                         |  post(Result{.., peaks, cleared})
    message thread: onLoadResult -> waveform_.apply() -> broadcaster_.sendChangeMessage()
                                                         v
                    GrainsEditor::changeListenerCallback -> status_ + view_.setWaveform(proc_.waveform())
    mouse: WaveformView -> xToPosition -> ParameterAttachment(position) -> host
    automation/sliders -> ParameterAttachment callbacks -> repaint()

Late-opened editor: the constructor pulls `proc_.waveform()`. Dropping a file on the view reaches the editor's `FileDragAndDropTarget` through the parent chain.

## Interfaces / Contracts

```cpp
namespace grains {
struct PeakBucket { float min = 0.f, max = 0.f; };
struct WaveformPeaks { std::vector<PeakBucket> buckets; };  // immutable once shared
inline constexpr std::size_t kDefaultPeakBuckets = 2048;
std::shared_ptr<const WaveformPeaks> makeWaveformPeaks(const ChannelView* ch, std::size_t numCh,
                                                       std::size_t numBuckets) noexcept;
PeakBucket columnPeak(const WaveformPeaks&, int column, int width) noexcept; // min/max over covered buckets
enum class WaveformDisplay { Empty, NoWaveform, Waveform };
struct WaveformState {                                     // message thread
    std::shared_ptr<const WaveformPeaks> peaks; bool hasSample = false;
    bool apply(bool loaded, bool cleared, std::shared_ptr<const WaveformPeaks> p) noexcept; // true if changed
    WaveformDisplay display() const noexcept;
};
// WaveformGeometry.h (header-only, inline noexcept)
float xToPosition(float x, float width); float positionToX(float p, float width);
struct Band { float lo, hi; }; Band jitterBand(float position, float jitter);
}
```

Bucket algorithm: bucket `b` covers frames `[b*F/N, (b+1)*F/N)`, computed in 64-bit integers. For each frame, mono = mean of the channels, and a non-finite sample counts as 0. When a bucket is empty (F < N), it uses the frame `min(b*F/N, F-1)`. The loop order is fixed, so results are deterministic. `numCh == 0`, `F == 0` or `N == 0` give empty buckets.

## File Changes

| File | Action | ~Lines |
|------|--------|-------|
| `Source/core/include/grains/WaveformPeaks.h`, `src/WaveformPeaks.cpp` | Create (peaks, columnPeak, WaveformState) | 120 |
| `Source/core/include/grains/WaveformGeometry.h` | Create (header-only) | 45 |
| `Source/core/CMakeLists.txt`, `tests/CMakeLists.txt`, `Source/plugin/CMakeLists.txt` | Register sources | 6 |
| `tests/test_waveform_peaks.cpp`, `test_waveform_geometry.cpp` | Create | 200 |
| `tests/test_alloc_failure.cpp` | Add a `makeWaveformPeaks` sweep (behind `GRAINS_ALLOC_SWEEPS`) | 30 |
| `Source/plugin/SampleLoader.h/.cpp` | Result fields, compute, `post` args | 25 |
| `Source/plugin/PluginProcessor.h/.cpp` | `WaveformState waveform_`, `waveform()`, `parameter(id)` accessor, apply in `onLoadResult` | 25 |
| `Source/plugin/WaveformView.h/.cpp` | Create (paint, mouse, attachments, hints) | 130 |
| `Source/plugin/PluginEditor.h/.cpp` | View member, layout, size, listener | 20 |

Code and tests come to about 600 lines. With the spec deltas (~195) the total is **about 800 lines, right at the 800 budget, so the risk is Medium-High**. Fallback if it runs over: two chained PRs, (1) core, tests and specs, (2) plugin glue.

## Testing Strategy

| Layer | What | Approach |
|-------|------|----------|
| Automated (`grains_tests`) | R-WD-1 scenarios, `columnPeak` coverage/width 0, R-WD-2 scenarios, `WaveformState` transitions (loaded/cleared/failed keeps/null peaks gives NoWaveform) | Catch2, written RED first |
| Automated (`grains_alloc_failure_tests`) | Fail the nth allocation: returns null, never throws | Existing `armFailure` harness |
| Unchanged | Realtime, determinism, and the R-BH-1 JUCE-free check | Existing suites stay green |
| Manual | R-WD-3/R-WD-4 scenarios: paint, gestures, automation follow, hints, editor open during load, fixed size, bit-identical audio | Standalone and a DAW, on Windows |

## Threat Matrix

N/A. There is no routing, shell, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary.

## Migration / Rollout

No migration is required. There are no state or parameter format changes.

## Open Questions

- [ ] D5 refines R-WD-3's "generation gate" wording: it uses commit order rather than the label filter. The spec should say "a superseded load MUST NOT replace peaks of a later-committed source". This needs confirmation in the spec or the verify phase.
