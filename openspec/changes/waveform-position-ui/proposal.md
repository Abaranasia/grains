# Proposal: Waveform and Position UI (roadmap #2)

## Intent
Users cannot see the loaded sample or where grains are read from; `position` is a blind 0..1 slider. Show the sample waveform with the position marker and read-only jitter band, and let click/drag set `position` directly.

## Scope
### In Scope
- Core (JUCE-free): `WaveformPeaks` (min/max downsample, mixed mono) and `WaveformGeometry` (x <-> normalised position, jitter band clamped to [0,1]), Catch2-tested.
- Peaks computed on the loader thread, delivered as immutable `shared_ptr` in `SampleLoader::Result`; held by the processor, generation-gated, cleared on Restore clear/missing file, redelivered on rate Reload.
- `WaveformView` above the generic editor; click/drag sets `position` via `ParameterAttachment` with change gestures; marker follows automation. Fixed-size window, taller.

### Out of Scope
- Live active-grain display (follow-up: GrainSnapshot + engine hook); core types stay mode-agnostic.
- Jitter dragging, new parameters, zoom/scroll, stacked stereo, playhead, theming, WebView (#5).

## Capabilities
### New Capabilities
- `waveform-display`: peaks contract, geometry mapping, delivery/lifecycle, view interaction.
### Modified Capabilities
- `plugin-shell`: R-PS-6 allows the waveform/position view; drop "waveform display" from Out of scope. R-PS-3 unchanged (9 params).

## Approach
Exploration approach A (native JUCE, thin glue). Logic in core; editor only paints and maps mouse through geometry. No locks, no `SourceExchange` or audio-path change. Repaint on change only (no Timer needed).

## Affected Areas
| Area | Impact | Description |
|------|--------|-------------|
| `Source/core/WaveformPeaks.*`, `WaveformGeometry.*` | New | Pure logic |
| `Tests/` + CMake | New/Modified | Catch2 cases, registration |
| `Source/plugin/SampleLoader.*` | Modified | Compute/attach peaks |
| `Source/plugin/PluginProcessor.*` | Modified | Hold/clear peaks, notify |
| `Source/plugin/PluginEditor.*`, `WaveformView.*` | New/Modified | Layout, view, attachment |
| `openspec/specs/plugin-shell`, `waveform-display` | Modified/New | Deltas |

Forecast: ~560-700 changed lines (core+tests ~300, plugin glue ~250, specs ~120); 800-line budget risk: Medium, single PR.

## Risks
| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Peaks allocation failure blocks a valid load | Low | Peaks optional: load publishes, view shows "no waveform" |
| Stale peaks after superseded load/clear | Med | Reuse generation gate; Catch2 where host-free |
| UI behaviour untestable in Catch2 | High | Manual scenarios: paint, drag gestures, automation follow, editor open during load, resize/DPI |
| Gesture misuse breaks automation recording | Low | begin/end gesture per drag |

## Rollback Plan
Revert the single PR; no state/parameter format change, so sessions stay compatible.

## Dependencies
- None new (JUCE `gui_basics`, Catch2 already present).

## Success Criteria
- [ ] Core peaks/geometry Catch2 tests pass; core stays JUCE-free; realtime/alloc suites unchanged.
- [ ] Loaded sample shows a waveform; click/drag moves `position` and records host automation.
- [ ] Jitter band tracks `positionJitter`; marker follows automation.
- [ ] Late-opened editor shows current waveform; clear/missing file empties it.
- [ ] Audio output bit-identical to before.

## Open assumptions (user review)
- Click jumps to the clicked position (absolute), drag continues from there.
- Peaks failure never blocks audio publish.
- Empty state shows a "drop a sample" hint.
